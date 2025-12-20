#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>

#include <zephyr/logging/log.h>
#include <zephyr/init.h>

LOG_MODULE_REGISTER(watchdog_setup, LOG_LEVEL_INF);

static const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));

static int wdt_channel_id;

static int setup_watchdog(void)
{
	int err;

	if (!device_is_ready(wdt)) {
		LOG_ERR("Watchdog device not ready.");
		return -ENODEV;
	}

	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Expire watchdog after 1000 ms */
		.window.min = 0,
		.window.max = 1000,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id < 0) {
		LOG_ERR("Watchdog install error: %d", wdt_channel_id);
		return wdt_channel_id;
	}

	err = wdt_setup(wdt, 0);
	if (err < 0) {
		LOG_ERR("Watchdog setup error: %d", err);
		return err;
	}

	LOG_INF("Watchdog initialized, channel id: %d", wdt_channel_id);
	return 0;
};

void feed_watchdog(void)
{
	int err = wdt_feed(wdt, wdt_channel_id);
	if (err < 0) {
		LOG_ERR("Watchdog feed error: %d", err);
	}
}

SYS_INIT(setup_watchdog, POST_KERNEL, 0);
