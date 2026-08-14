/*
 * bstarship.c - STARSHIP builtin
 *
 * Integrates the Starship cross-shell prompt (https://starship.rs)
 * with this cmd shell.  When enabled, the interactive prompt is
 * rendered by running `starship prompt` as an external program,
 * mirroring how `starship init` hooks other shells.
 *
 *   STARSHIP [init|off] [/?]
 *
 * License: GNU GPLv3
 */

#include "ccontext.h"
#include "glibcmd.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* -------------------------------------------------------------------------
 * Monotonic millisecond clock (POSIX).  Falls back to wall-clock time,
 * so command duration is still sane on systems without CLOCK_MONOTONIC.
 * ---------------------------------------------------------------------- */

long long cmd_starship_monotonic_ms(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
    return (long long)time(NULL) * 1000;
}

/* -------------------------------------------------------------------------
 * Run `starship prompt` and capture its output.
 *
 * Writes at most out_size-1 bytes (NUL-terminated) into out and returns
 * the byte length, or -1 when starship is unavailable.
 * ---------------------------------------------------------------------- */

int cmd_starship_prompt(cmd_context_t *ctx, char *out, size_t out_size)
{
    char exec_path[CMD_MAX_PATH];
    char cmd[512];
    FILE *fp;
    size_t total = 0;
    long long dur = 0;
    int cols = 0;
    int n;

    if (out == NULL || out_size == 0)
        return -1;
    out[0] = '\0';

    if (libcmd_find_exec("starship", libcmd_getenv("PATH"),
                         exec_path, sizeof(exec_path)) < 0)
        return -1;

    if (ctx->starship_line_start_ms > 0) {
        long long now = cmd_starship_monotonic_ms();
        if (now > ctx->starship_line_start_ms)
            dur = now - ctx->starship_line_start_ms;
    }

    libcmd_get_terminal_size(&cols, NULL);

    n = libcmd_sprintf_s(cmd, sizeof(cmd),
                         "%s prompt --status=%d --cmd-duration=%lld"
                         " --terminal-width=%d --keymap=emacs",
                         exec_path, ctx->exit_code, dur, cols);
    if (n < 0)
        return -1;

    fp = libcmd_popen(cmd, "r");
    if (fp == NULL)
        return -1;

    while (total < out_size - 1) {
        size_t r = fread(out + total, 1, out_size - 1 - total, fp);
        if (r == 0)
            break;
        total += r;
    }
    libcmd_pclose(fp);

    out[total] = '\0';
    return (int)total;
}

/* -------------------------------------------------------------------------
 * Builtin
 * ---------------------------------------------------------------------- */

int builtin_starship(cmd_context_t *ctx, int argc, char **argv)
{
    if (argc >= 2 && libcmd_strcasecmp(argv[1], "/?") == 0) {
        fputs(cmd_gettext(MSG_HELP_STARSHIP), stdout);
        return 0;
    }

    if (argc <= 1) {
        /* Report current state */
        fputs("starship prompt is ", stdout);
        fputs(cmd_gettext(ctx->starship ? MSG_ON : MSG_OFF), stdout);
        fputc('\n', stdout);
        return 0;
    }

    if (libcmd_strcasecmp(argv[1], "init") == 0) {
        char exec_path[CMD_MAX_PATH];

        if (libcmd_find_exec("starship", libcmd_getenv("PATH"),
                             exec_path, sizeof(exec_path)) < 0) {
            fputs(cmd_gettext(MSG_ERR_STARSHIP_NOT_FOUND), stderr);
            return 1;
        }

        ctx->starship = 1;
        ctx->starship_line_start_ms = 0;
        ctx->omp = 0;  /* starship and oh-my-posh are mutually exclusive */
        libcmd_setenv("STARSHIP_SHELL", "cmd", 1);
        if (libcmd_getenv("STARSHIP_SESSION_KEY") == NULL) {
            static const char chars[] =
                "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
            char key[17];
            int j;
            for (j = 0; j < 16; j++)
                key[j] = chars[libcmd_random() % (sizeof(chars) - 1)];
            key[16] = '\0';
            libcmd_setenv("STARSHIP_SESSION_KEY", key, 1);
        }
        return 0;
    }

    if (libcmd_strcasecmp(argv[1], "off") == 0 ||
        libcmd_strcasecmp(argv[1], "deinit") == 0 ||
        libcmd_strcasecmp(argv[1], "disable") == 0) {
        ctx->starship = 0;
        return 0;
    }

    /* Any other subcommand (toggle, config, session, explain, ...) is
     * delegated to the real starship binary. */
    {
        char exec_path[CMD_MAX_PATH];
        libcmd_exit_info_t exit_info;

        if (libcmd_find_exec("starship", libcmd_getenv("PATH"),
                             exec_path, sizeof(exec_path)) < 0) {
            fputs(cmd_gettext(MSG_ERR_STARSHIP_NOT_FOUND), stderr);
            return 1;
        }
        if (libcmd_exec_sync(exec_path, argv, libcmd_get_environ(),
                             -1, -1, -1, 0, &exit_info) < 0)
            return 1;
        return exit_info.exit_code;
    }
}
