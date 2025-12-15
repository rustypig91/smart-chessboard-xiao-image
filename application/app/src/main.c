
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include "watchdog.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	bool status = gpio_is_ready_dt(&led);
	__ASSERT(status, "Error: GPIO Device not ready");

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	bool led_state = false;

dtr_not_set:

	/* Poll if the DTR flag was set */
	while (!dtr) {
		feed_watchdog();

		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		/* Give CPU resources to low-priority threads. */
		gpio_pin_set_dt(&led, (int)led_state);
		led_state = !led_state;
		k_msleep(100);
	}

	while (1) {
		feed_watchdog();

		gpio_pin_set_dt(&led, (int)led_state);
		led_state = !led_state;
		k_msleep(500);

		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		if (!dtr) {
			goto dtr_not_set;
		}
	}

	k_sleep(K_FOREVER);

	return 0;
}
