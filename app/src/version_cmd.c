#include <zephyr/shell/shell.h>

#ifndef APP_VERSION_STRING
#error "APP_VERSION_STRING is not defined"
#endif

static int cmd_version(const struct shell *shell, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    shell_print(shell, "%s", APP_VERSION_STRING);
    return 0;
}

SHELL_CMD_REGISTER(version, NULL, "Show application version", cmd_version);
