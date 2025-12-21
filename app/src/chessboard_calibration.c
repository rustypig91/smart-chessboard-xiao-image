
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include "chessboard.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(chessboard_calibration, LOG_LEVEL_INF);

static int16_t calibration_offset_mv[8][8] = {
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
	{1650, 1650, 1650, 1650, 1650, 1650, 1650, 1650},
};

static int chessboard_calibration_set(const char *name, size_t len, settings_read_cb read_cb,
				      void *cb_arg);

struct settings_handler chessboard_calibration_setting = {.name = "calibration",
							  .h_set = chessboard_calibration_set};

int32_t chessboard_calibration_get_mv(uint8_t file, uint8_t rank)
{
	if (file > 7 || rank > 7) {
		return -1;
	}
	return (int32_t)calibration_offset_mv[rank][file];
}

int chessboard_calibration_calibrate(void)
{
	int ret = chessboard_scan();

	if (ret != 0) {
		return ret;
	}
	int32_t mv;
	for (int rank = 0; rank < 8; rank++) {
		for (int file = 0; file < 8; file++) {
			mv = chessboard_get_mv(file, rank);
			if (mv < INT16_MIN) {
				LOG_ERR("mv reading %d below int16 min at file %d, rank %d", mv,
					file, rank);
				return -EOVERFLOW;
			}

			if (mv > INT16_MAX) {
				LOG_ERR("mv reading %d exceeds int16 max at file %d, rank %d", mv,
					file, rank);
				return -EOVERFLOW;
			}

			calibration_offset_mv[rank][file] = (int16_t)mv;
		}
	}

	ret = settings_save_one("calibration/calibration", &calibration_offset_mv,
				sizeof(calibration_offset_mv));

	if (ret != 0) {
		LOG_ERR("Failed to save calibration data: %d", ret);
	} else {
		LOG_INF("Calibration data saved successfully");
	}

	return ret;
}

static int chessboard_calibration_set(const char *name, size_t len, settings_read_cb read_cb,
				      void *cb_arg)
{
	int rc;

	rc = read_cb(cb_arg, &calibration_offset_mv, sizeof(calibration_offset_mv));
	if (rc >= 0) {
		rc = 0;
	}

	return rc;
}

static int chessboard_calibration_init(void)
{
	int ret;
	ret = settings_subsys_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize settings subsystem: %d", ret);
		return ret;
	}

	ret = settings_register(&chessboard_calibration_setting);
	if (ret != 0) {
		LOG_ERR("Failed to register settings handler: %d", ret);
		return ret;
	}
	ret = settings_load();
	if (ret != 0) {
		LOG_WRN("No calibration data found, using defaults");
	} else {
		LOG_INF("Calibration data loaded successfully");
	}

	return 0;
}

SYS_INIT(chessboard_calibration_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
