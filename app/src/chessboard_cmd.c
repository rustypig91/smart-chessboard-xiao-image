#include "chessboard.h"
#include "chessboard_calibration.h"
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include <string.h>

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
		for (int file = 0; file < CHESS_NUM_FILES; file++) {
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
	for (int rank = 0; rank < CHESS_NUM_RANKS; rank++) {
		shell_fprintf(sh, SHELL_NORMAL, "|");
		val_mv = get_value_func(file, rank);
		shell_fprintf(sh, SHELL_NORMAL, "%4d", val_mv);
	}
	shell_fprintf(sh, SHELL_NORMAL, "\n");
	return 0;
}

static bool monitor_command_shall_quit(const struct shell *sh)
{
	char c;
	int ret;
	size_t cnt;

	ret = sh->iface->api->read(sh->iface, &c, sizeof(c), &cnt);
	if (ret != 0) {
		shell_error(sh, "Failed to read from shell (%d)", ret);
		return true;
	}

	if (cnt == 1 && (c == 'q' || c == 'Q')) {
		return true;
	}

	return false;
}

static int monitor_file(const struct shell *sh, int32_t (*get_value_func)(uint8_t, uint8_t))
{
	while (1) {
		for (uint8_t file = 0; file < CHESS_NUM_FILES; file++) {
			print_mv_file(sh, get_value_func, file);
		}

		if (monitor_command_shall_quit(sh)) {
			break;
		}

		k_yield();
	}
	return 0;
}

static int monitor_offset_threshold(const struct shell *sh, int32_t negative_threshold_mv,
				    int32_t positive_threshold_mv, int32_t hysteresis_mv)
{
#define STATE_NEGATIVE 0
#define STATE_POSITIVE 1
#define STATE_NEUTRAL  2
#define STATE_UNKNOWN  3

	uint8_t prev_state[CHESS_NUM_FILES][CHESS_NUM_RANKS];
	for (int file = 0; file < CHESS_NUM_FILES; file++) {
		for (int rank = 0; rank < CHESS_NUM_RANKS; rank++) {
			prev_state[file][rank] = STATE_UNKNOWN;
		}
	}

	uint8_t file = 0;

	while (1) {
		chessboard_scan_file(file);

		for (uint8_t rank = 0; rank < CHESS_NUM_RANKS; rank++) {
			const int32_t mv = chessboard_get_mv_offset(file, rank);

			int32_t negative = negative_threshold_mv;
			int32_t positive = positive_threshold_mv;

			if (prev_state[file][rank] == STATE_POSITIVE) {
				negative -= hysteresis_mv;
				positive -= hysteresis_mv;
			} else if (prev_state[file][rank] == STATE_NEGATIVE) {
				negative += hysteresis_mv;
				positive += hysteresis_mv;
			} else if (prev_state[file][rank] == STATE_NEUTRAL) {
				negative -= hysteresis_mv;
				positive += hysteresis_mv;
			}

			const bool above = mv > positive;
			const bool below = mv < negative;
			const bool within = !above && !below;
			if ((prev_state[file][rank] != STATE_POSITIVE) && above) {
				shell_print(sh, "+%c%d", 'A' + file, rank + 1);
				prev_state[file][rank] = STATE_POSITIVE;
			} else if ((prev_state[file][rank] != STATE_NEGATIVE) && below) {
				shell_print(sh, "-%c%d", 'A' + file, rank + 1);
				prev_state[file][rank] = STATE_NEGATIVE;
			} else if ((prev_state[file][rank] != STATE_NEUTRAL) && within) {
				shell_print(sh, " %c%d", 'A' + file, rank + 1);
				prev_state[file][rank] = STATE_NEUTRAL;
			} else {
				// no state change
			}
		}

		if (monitor_command_shall_quit(sh)) {
			break;
		}

		file = (file + 1) % CHESS_NUM_FILES;
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

static int cmd_board_monitor_offset_threshold(const struct shell *sh, size_t argc, char **argv)
{
	if ((argc < 3) || (argc > 4)) {
		shell_print(sh, "Invalid number of arguments (%d)", argc);
		shell_print(
			sh,
			"Usage: board monitor threshold <negative_threshold_mV> "
			"<positive_threshold_mV> [hysteresis_mV]\n"
			"- <negative_threshold_mV>: threshold for negative offset notification\n"
			"- <positive_threshold_mV>: threshold for positive offset notification\n"
			"- [hysteresis_mV]: optional hysteresis value to avoid notification "
			"flooding\n");

		return -EINVAL;
	}

	int err = 0;
	long negative_threshold_mv = shell_strtol(argv[1], 10, &err);

	if (err != 0) {
		shell_error(sh, "Invalid negative threshold: %s", argv[1]);
		return err;
	} else if ((negative_threshold_mv < INT32_MIN) || (negative_threshold_mv > INT32_MAX)) {
		shell_error(sh, "Negative threshold out of range: %s", argv[1]);
		return -ERANGE;
	}

	long positive_threshold_mv = shell_strtol(argv[2], 10, &err);
	if (err != 0) {
		shell_error(sh, "Invalid positive threshold: %s", argv[2]);
		return err;
	} else if ((positive_threshold_mv < INT32_MIN) || (positive_threshold_mv > INT32_MAX) ||
		   (positive_threshold_mv <= negative_threshold_mv)) {
		shell_error(sh, "Positive threshold out of range: %s", argv[2]);
		return -ERANGE;
	}

	unsigned long hysteresis_mv = 0;
	if (argc >= 4) {
		hysteresis_mv = shell_strtoul(argv[3], 10, &err);
		if (err != 0) {
			shell_error(sh, "Invalid hysteresis: %s", argv[3]);
			return err;
		} else if (hysteresis_mv > INT32_MAX) {
			shell_error(sh, "Hysteresis too large: %s", argv[3]);
			return -ERANGE;
		}
	}

	return monitor_offset_threshold(sh, (int32_t)negative_threshold_mv,
					(int32_t)positive_threshold_mv, (int32_t)hysteresis_mv);
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

SHELL_STATIC_SUBCMD_SET_CREATE(calib_cmds,
			       SHELL_CMD(get, NULL, "Print the chess board calibration",
					 cmd_print_board_calibration),
			       SHELL_CMD(set, NULL, "Set the chess board calibration",
					 cmd_set_board_calibration),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	monitor,
	SHELL_CMD(voltage, NULL, "Monitor chess board voltages", cmd_board_monitor_file_voltage),
	SHELL_CMD(offset, NULL, "Monitor chess board offset voltages",
		  cmd_board_monitor_file_offset_voltage),
	SHELL_CMD(threshold, NULL,
		  "Notify with hysteresis: threshold <negative_mV> <positive_mV> [hysteresis_mV]",
		  cmd_board_monitor_offset_threshold),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	chess_cmds,
	SHELL_CMD(voltage, NULL, "Print the chess board voltages in millivolts",
		  cmd_print_board_voltage),
	SHELL_CMD(offset, NULL,
		  "Print the chess board voltage offset from calibration value in millivolts",
		  cmd_print_board_offset_voltage),
	SHELL_CMD(calib, &calib_cmds, "Chess board calibration commands", NULL),
	SHELL_CMD(monitor, &monitor, "Monitor chess board values", NULL), SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(board, &chess_cmds, "Chess board commands", NULL);
