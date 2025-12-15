#pragma once

#include <stdint.h>

#define CHESS_FILE_A 0
#define CHESS_FILE_B 1
#define CHESS_FILE_C 2
#define CHESS_FILE_D 3
#define CHESS_FILE_E 4
#define CHESS_FILE_F 5
#define CHESS_FILE_G 6
#define CHESS_FILE_H 7

#define CHESS_RANK_1 0
#define CHESS_RANK_2 1
#define CHESS_RANK_3 2
#define CHESS_RANK_4 3
#define CHESS_RANK_5 4
#define CHESS_RANK_6 5
#define CHESS_RANK_7 6
#define CHESS_RANK_8 7

typedef enum {
	CHESS_PIECE_NONE,
	CHESS_PIECE_BLACK,
	CHESS_PIECE_WHITE
} CHESS_PIECE;

int chessboard_scan_file(uint8_t file);
int chessboard_scan(void);
int chessboard_calibrate(void);
int32_t chessboard_get_mv(uint8_t file, uint8_t rank);
int32_t chessboard_get_mv_offset(uint8_t file, uint8_t rank);



CHESS_PIECE chessboard_get_color(uint8_t file, uint8_t rank);

