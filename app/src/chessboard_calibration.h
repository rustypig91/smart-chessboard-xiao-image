#pragma once

#include <stdint.h>


int32_t chessboard_calibration_get_mv(uint8_t file, uint8_t rank);
int chessboard_calibration_calibrate(void);
