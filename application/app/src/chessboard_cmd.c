#include "chessboard.h"
#include "chessboard_calibration.h"
#include "zephyr/kernel.h"
#include <zephyr/console/console.h>
#include <zephyr/shell/shell.h>

char *shell_getline(const struct shell *shell, char *buf, const size_t len)
{
	if (!buf) {
		return NULL;
	}

	char *p = buf;
	memset(buf, 0, len);

	for (size_t i = 0; i < (len - 1); i++) {
		char c;
		size_t cnt;

		shell->iface->api->read(shell->iface, &c, sizeof(c), &cnt);
		while (cnt == 0) {
			k_busy_wait(100);
			shell->iface->api->read(shell->iface, &c, sizeof(c), &cnt);
		}
		shell->iface->api->write(shell->iface, &c, sizeof(c), &cnt);
		if (c == '\n' || c == '\r') {
			break; // end of line
		}
		*p++ = c;
	}

	return buf;
}

static int print_mv(const struct shell *sh, int32_t (*get_value_func)(uint8_t, uint8_t))
{
	int32_t val_mv;
	chessboard_scan();

	for (int rank = 7; rank >= 0; rank--) {
		shell_fprintf(sh, SHELL_NORMAL, "%d", rank + 1);
		for (int file = 0; file < 8; file++) {
			shell_fprintf(sh, SHELL_NORMAL, "|");
			val_mv = get_value_func(file, rank);
			shell_fprintf(sh, SHELL_NORMAL, "%4d", val_mv);
		}
		shell_fprintf(sh, SHELL_NORMAL, "\n");
	}
	shell_fprintf(sh, SHELL_NORMAL, "\n");
	return 0;
}

static int print_mv_file(const struct shell *sh, int32_t (*get_value_func)(uint8_t, uint8_t),
			 uint8_t file)
{
	int32_t val_mv;
	chessboard_scan_file(file);

	shell_fprintf(sh, SHELL_NORMAL, "%c", 'A' + file);
	for (int rank = 0; rank < 8; rank++) {
		shell_fprintf(sh, SHELL_NORMAL, "|");
		val_mv = get_value_func(file, rank);
		shell_fprintf(sh, SHELL_NORMAL, "%4d", val_mv);
	}
	shell_fprintf(sh, SHELL_NORMAL, "\n");
	return 0;
}

static int monitor_file(const struct shell *sh, int32_t (*get_value_func)(uint8_t, uint8_t))
{
	char c;
	int ret;
	size_t cnt;
	while (1) {
		for (uint8_t file = CHESS_FILE_A; file <= CHESS_FILE_H; file++) {
			print_mv_file(sh, get_value_func, file);
		}

		ret = sh->iface->api->read(sh->iface, &c, sizeof(c), &cnt);
		if (ret != 0) {
			shell_error(sh, "Failed to read from shell (%d)", ret);
			return ret;
		}

		if (cnt == 1 && (c == 'q' || c == 'Q')) {
			break;
		}

		k_yield();
	}
	return 0;
}

static int cmd_board_monitor_file_voltage(const struct shell *sh, size_t argc, char **argv)
{
	monitor_file(sh, chessboard_get_mv);
	return 0;
}

static int cmd_board_monitor_file_offset_voltage(const struct shell *sh, size_t argc, char **argv)
{
	monitor_file(sh, chessboard_get_mv_offset);
	return 0;
}

static int cmd_print_board_voltage(const struct shell *sh, size_t argc, char **argv)
{
	print_mv(sh, chessboard_get_mv);
	return 0;
}

static int cmd_print_board_offset_voltage(const struct shell *sh, size_t argc, char **argv)
{
	print_mv(sh, chessboard_get_mv_offset);
	return 0;
}

static int cmd_print_board_calibration(const struct shell *sh, size_t argc, char **argv)
{
	print_mv(sh, chessboard_calibration_get_mv);
	return 0;
}

static int cmd_set_board_calibration(const struct shell *sh, size_t argc, char **argv)
{
	int ret = chessboard_calibration_calibrate();
	if (ret != 0) {
		shell_print(sh, "Calibration failed: %d", ret);
		return ret;
	}

	shell_print(sh, "Calibration complete");
	return 0;
}

static int cmd_print_board_pieces(const struct shell *sh, size_t argc, char **argv)
{
	for (int rank = 7; rank >= 0; rank--) {
		shell_fprintf(sh, SHELL_NORMAL, "%d |", rank + 1);
		for (int file = 0; file < 8; file++) {
			CHESS_PIECE piece = chessboard_get_color(file, rank);
			char color = piece == CHESS_PIECE_NONE
					     ? ' '
					     : (piece == CHESS_PIECE_BLACK ? 'B' : 'W');

			shell_fprintf(sh, SHELL_NORMAL, "  %c  |", color);
		}
		shell_fprintf(sh, SHELL_NORMAL, "\n");
	}

	shell_fprintf(sh, SHELL_NORMAL, "-------------------------------\n");
	shell_fprintf(sh, SHELL_NORMAL,
		      //    1 |  B  |  W  |     |     |     |     |     |     |
		      "     A    B    C    D    E    F    G    H\n");

	shell_fprintf(sh, SHELL_NORMAL, "\n");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(calib_cmds,
			       SHELL_CMD(get, NULL, "Print the chess board calibration",
					 cmd_print_board_calibration),
			       SHELL_CMD(set, NULL, "Set the chess board calibration",
					 cmd_set_board_calibration),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(monitor,
			       SHELL_CMD(voltage, NULL, "Monitor chess board voltages",
					 cmd_board_monitor_file_voltage),
			       SHELL_CMD(offset, NULL, "Monitor chess board offset voltages",
					 cmd_board_monitor_file_offset_voltage),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	chess_cmds,
	SHELL_CMD(voltage, NULL, "Print the chess board voltages in millivolts",
		  cmd_print_board_voltage),
	SHELL_CMD(offset, NULL,
		  "Print the chess board voltage offset from calibration value in millivolts",
		  cmd_print_board_offset_voltage),
	SHELL_CMD(calib, &calib_cmds, "Chess board calibration commands", NULL),
	SHELL_CMD(pieces, NULL, "Print the chess board pieces", cmd_print_board_pieces),
	SHELL_CMD(monitor, &monitor, "Monitor chess board values", NULL), SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(board, &chess_cmds, "Chess board commands", NULL);
