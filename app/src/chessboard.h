#pragma once

#include <stdint.h>

#define CHESS_FILE_A    ((uint8_t)0u)
#define CHESS_FILE_B    ((uint8_t)1u)
#define CHESS_FILE_C    ((uint8_t)2u)
#define CHESS_FILE_D    ((uint8_t)3u)
#define CHESS_FILE_E    ((uint8_t)4u)
#define CHESS_FILE_F    ((uint8_t)5u)
#define CHESS_FILE_G    ((uint8_t)6u)
#define CHESS_FILE_H    ((uint8_t)7u)
#define CHESS_NUM_FILES ((uint8_t)8u)

#define CHESS_RANK_1    ((uint8_t)0u)
#define CHESS_RANK_2    ((uint8_t)1u)
#define CHESS_RANK_3    ((uint8_t)2u)
#define CHESS_RANK_4    ((uint8_t)3u)
#define CHESS_RANK_5    ((uint8_t)4u)
#define CHESS_RANK_6    ((uint8_t)5u)
#define CHESS_RANK_7    ((uint8_t)6u)
#define CHESS_RANK_8    ((uint8_t)7u)
#define CHESS_NUM_RANKS ((uint8_t)8u)

int chessboard_scan_file(uint8_t file);
int chessboard_scan(void);
int chessboard_calibrate(void);
int32_t chessboard_get_mv(uint8_t file, uint8_t rank);
int32_t chessboard_get_mv_offset(uint8_t file, uint8_t rank);
