#include "chessboard.h"
#include "chessboard_calibration.h"

#include <zephyr/kernel.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(chessboard, LOG_LEVEL_INF);

#define HAL_NUM_MEASUREMENTS            (uint8_t)1

#define GET_INDEX(file, rank) ((rank) * 8 + (file))

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

static const struct gpio_dt_spec channel_select_a =
	GPIO_DT_SPEC_GET(DT_ALIAS(channel_select_a), gpios);
static const struct gpio_dt_spec channel_select_b =
	GPIO_DT_SPEC_GET(DT_ALIAS(channel_select_b), gpios);
static const struct gpio_dt_spec channel_select_c =
	GPIO_DT_SPEC_GET(DT_ALIAS(channel_select_c), gpios);

/* Mapping of chess board rows to multiplexer channels */
static uint8_t multiplexer_mapping[8][8] = {
	// [Multiplexer][Channel] = Board row
	{2, 1, 0, 3, 4, 7, 5, 6}, // A - Normal
	{6, 5, 4, 7, 3, 0, 2, 1}, // B - Mirrored
	{2, 1, 0, 3, 4, 7, 5, 6}, // C - Normal
	{6, 5, 4, 7, 3, 0, 2, 1}, // D - Mirrored
	{1, 2, 3, 0, 4, 7, 5, 6}, // E - Normal
	{6, 5, 4, 7, 3, 0, 2, 1}, // F - Mirrored
	{1, 2, 3, 0, 4, 7, 5, 6}, // G - Normal
	{6, 5, 4, 7, 3, 0, 2, 1}, // H - Mirrored
};

static bool hal_sensor_inverted[8] = {
	false, true, false, true, false, true, false, true,
};

/* Data of ADC io-channels specified in devicetree. */
static const struct adc_dt_spec adc_channels[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels, DT_SPEC_AND_COMMA)};

static int32_t chess_pieces_mv[64] = {0};

static int select_multiplexer_channel(uint8_t channel);
static int read_adc_channel(const int index, int32_t *val_mv, uint8_t num_measurements);

int chessboard_scan_file(uint8_t file)
{
	if (file > 7) {
		return -EINVAL;
	}

	int32_t offset;
	for (int multiplexer_channel = 0; multiplexer_channel < 8; multiplexer_channel++) {

		int ret = select_multiplexer_channel(multiplexer_channel);

		if (ret != 0) {
			LOG_ERR("Failed to select multiplexer channel %d: %d", multiplexer_channel,
				ret);
			return ret;
		}

		int32_t val_mv;
		ret = read_adc_channel(file, &val_mv, HAL_NUM_MEASUREMENTS);
		if (ret < 0) {
			LOG_ERR("Failed to read ADC channel %d (err %d)", file, ret);
			return ret;
		}

		if (hal_sensor_inverted[file]) {
			val_mv = -val_mv;
		}

		int rank = multiplexer_mapping[file][multiplexer_channel];

		const int index = GET_INDEX(file, rank);

		chess_pieces_mv[index] = val_mv;
		offset = chessboard_calibration_get_mv(file, rank) - val_mv;


		if (ret != 0) {
			LOG_ERR("Error reading square %02d: %d", index, ret);
		}
	}
	return 0;
}

int chessboard_scan(void)
{
	int32_t offset;
	for (int multiplexer_channel = 0; multiplexer_channel < 8; multiplexer_channel++) {

		int ret = select_multiplexer_channel(multiplexer_channel);

		if (ret != 0) {
			LOG_ERR("Failed to select multiplexer channel %d: %d", multiplexer_channel,
				ret);
			return ret;
		}

		for (int file = 0; file < 8; file++) {

			int32_t val_mv;
			ret = read_adc_channel(file, &val_mv, HAL_NUM_MEASUREMENTS);
			if (ret < 0) {
				LOG_ERR("Failed to read ADC channel %d (err %d)", file, ret);
				return ret;
			}

			if (hal_sensor_inverted[file]) {
				val_mv = -val_mv;
			}

			int rank = multiplexer_mapping[file][multiplexer_channel];

			const int index = GET_INDEX(file, rank);

			chess_pieces_mv[index] = val_mv;
			offset = chessboard_calibration_get_mv(file, rank) - val_mv;

			if (ret != 0) {
				LOG_ERR("Error reading square %02d: %d", index, ret);
			}
		}
	}
	return 0;
}

int32_t chessboard_get_mv(uint8_t file, uint8_t rank)
{
	if (file > 7 || rank > 7) {
		return -1;
	}

	return chess_pieces_mv[GET_INDEX(file, rank)];
}

int32_t chessboard_get_mv_offset(uint8_t file, uint8_t rank)
{
	if (file > 7 || rank > 7) {
		return -1;
	}
	int index = GET_INDEX(file, rank);
	return chessboard_calibration_get_mv(file, rank) - chess_pieces_mv[index];
}

static int select_multiplexer_channel(uint8_t channel)
{
	int ret;
	const struct gpio_dt_spec *channels[] = {&channel_select_a, &channel_select_b,
						 &channel_select_c};

	static bool is_initialized = false;
	if (!is_initialized) {
		for (int i = 0; i < 3; i++) {
			if (!gpio_is_ready_dt(channels[i])) {
				return -ENODEV;
			}
			ret = gpio_pin_configure_dt(channels[i], GPIO_OUTPUT_INACTIVE);
			if (ret < 0) {
				return -ENODEV;
			}
		}
		is_initialized = true;
		LOG_INF("Multiplexer channel select pins initialized\n");
	}

	for (int i = 0; i < 3; i++) {
		int value = (channel >> i) & 0x01;
		ret = gpio_pin_set_dt(channels[i], value);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

static int read_adc_channel(const int index, int32_t *val_mv, uint8_t num_measurements)
{
	if (index >= ARRAY_SIZE(adc_channels)) {
		LOG_ERR("ADC channel index %d out of range", index);
		return 0;
	}

	struct adc_sequence sequence = {
		.buffer = val_mv,
		.buffer_size = sizeof(*val_mv),
	};

	if (!adc_is_ready_dt(&adc_channels[index])) {
		LOG_WRN("ADC controller device %s not ready\n", adc_channels[index].dev->name);
		return 0;
	}

	int ret = adc_channel_setup_dt(&adc_channels[index]);
	if (ret < 0) {
		LOG_ERR("Could not setup channel #%d (%d)\n", index, ret);
		return 0;
	}

	ret = adc_sequence_init_dt(&adc_channels[index], &sequence);
	if (ret != 0) {
		LOG_ERR("Could not init sequence for channel #%d (%d)\n", index, ret);
		return ret;
	}

	*val_mv = 0;

	// for (uint8_t i = 0; i < num_measurements; i++) {
		ret = adc_read_dt(&adc_channels[index], &sequence);
		if (ret != 0) {
			LOG_ERR("Failed to read ADC channel %d (err %d)", index, ret);
			return ret;
		}
		// *val_mv += buf;
	// }
	// *val_mv /= num_measurements;

	return adc_raw_to_millivolts_dt(&adc_channels[index], val_mv);
}

static int chessboard_setup()
{
	int err = select_multiplexer_channel(0);
	if (err != 0) {
		LOG_ERR("Failed to setup chessboard multiplexer: %d", err);
	}
	return err;
}

SYS_INIT(chessboard_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
