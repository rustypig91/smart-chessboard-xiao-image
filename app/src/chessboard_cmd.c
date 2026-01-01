#include "chessboard.h"
#include "chessboard_calibration.h"
#include <zephyr/kernel.h>
#include <zephyr/console/console.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>

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
		for (uint8_t file = CHESS_FILE_A; file <= CHESS_FILE_H; file++) {
			print_mv_file(sh, get_value_func, file);
		}

		if (monitor_command_shall_quit(sh)) {
			break;
		}

		k_yield();
	}
	return 0;
}

static int monitor_offset_threshold(const struct shell *sh, int32_t threshold_mv,
				    uint32_t interval_ms)
{
#define STATE_BELOW_THRESHOLD  0
#define STATE_ABOVE_THRESHOLD  1
#define STATE_WITHIN_THRESHOLD 2
#define STATE_UNKNOWN          3

	uint8_t prev_state[8][8];
	for (int f = 0; f < 8; f++) {
		for (int r = 0; r < 8; r++) {
			prev_state[f][r] = STATE_UNKNOWN;
		}
	}

	uint8_t file = 0;

	while (1) {
		chessboard_scan_file(file);

		for (uint8_t rank = CHESS_RANK_1; rank <= CHESS_RANK_8; rank++) {
			int32_t mv = chessboard_get_mv_offset(file, rank);
			if (mv < 0) {
				mv = -mv;
			}

			const bool above = mv > threshold_mv;
			const bool below = mv < threshold_mv;
			if (prev_state[file][rank] == STATE_UNKNOWN) {
				// First reading, just set the state
				if (above) {
					prev_state[file][rank] = STATE_ABOVE_THRESHOLD;
				} else if (below) {
					prev_state[file][rank] = STATE_BELOW_THRESHOLD;
				} else {
					prev_state[file][rank] = STATE_WITHIN_THRESHOLD;
				}
			} else if (prev_state[file][rank] != STATE_ABOVE_THRESHOLD && above) {
				shell_print(sh,
					    "Offset crossed above threshold on %c%d: %d mV",
					    'A' + file, rank + 1,
					    chessboard_get_mv_offset(file, rank));
				prev_state[file][rank] = STATE_ABOVE_THRESHOLD;
			} else if (prev_state[file][rank] != STATE_BELOW_THRESHOLD && below) {
				shell_print(sh,
					    "Offset crossed below threshold on %c%d: %d mV",
					    'A' + file, rank + 1,
					    chessboard_get_mv_offset(file, rank));
				prev_state[file][rank] = STATE_BELOW_THRESHOLD;
			} else if (prev_state[file][rank] != STATE_WITHIN_THRESHOLD && !above && !below) {
				shell_print(sh,
					    "Offset within threshold on %c%d: %d mV",
					    'A' + file, rank + 1,
					    chessboard_get_mv_offset(file, rank));
				prev_state[file][rank] = STATE_WITHIN_THRESHOLD;
			}
		}

		if (monitor_command_shall_quit(sh)) {
			break;
		}

		k_yield();
		file = (file + 1) % 8;
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
	if (argc < 2) {
		shell_print(sh, "Usage: board monitor threshold <mV> [interval_ms]\n"
				"- <mV>: threshold in millivolts for absolute offset\n"
				"- [interval_ms]: optional polling interval, default yield");
		return -EINVAL;
	}

	char *end;
	long threshold = strtol(argv[1], &end, 10);
	if (*end != '\0') {
		shell_error(sh, "Invalid threshold: %s", argv[1]);
		return -EINVAL;
	}

	uint32_t interval_ms = 0;
	if (argc >= 3) {
		long interval = strtol(argv[2], &end, 10);
		if (*end != '\0' || interval < 0) {
			shell_error(sh, "Invalid interval: %s", argv[2]);
			return -EINVAL;
		}
		interval_ms = (uint32_t)interval;
	}

	shell_print(sh, "Monitoring offset crossings > %ld mV. Press 'q' to quit.", threshold);
	return monitor_offset_threshold(sh, (int32_t)threshold, interval_ms);
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
		  "Notify when offset crosses threshold: threshold <mV> [interval_ms]",
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
