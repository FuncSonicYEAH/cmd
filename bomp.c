/*
 * bomp.c - OH-MY-POSH builtin
 *
 * Integrates the Oh My Posh cross-shell prompt (https://ohmyposh.dev)
 * with this cmd shell.  When enabled, the interactive prompt is
 * rendered by running `oh-my-posh print primary` as an external program,
 * mirroring how the Starship prompt is integrated.
 *
 *   OMP [init [config]|off] [/?]
 *
 * License: GNU GPLv3
 */

#include "ccontext.h"
#include "glibcmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Run `oh-my-posh print primary` and capture its output.
 *
 * Writes at most out_size-1 bytes (NUL-terminated) into out and returns
 * the byte length, or -1 when oh-my-posh is unavailable.  Newer oh-my-posh
 * releases accept --terminal-width/--escape; older ones error out on
 * unknown flags, so a second attempt drops the newer flags.
 * ---------------------------------------------------------------------- */

int cmd_omp_prompt(cmd_context_t *ctx, char *out, size_t out_size)
{
    char exec_path[CMD_MAX_PATH];
    char cmd[2048];
    FILE *fp;
    size_t total = 0;
    long long dur = 0;
    int cols = 0;
    int rc;
    const char *config;

    if (out == NULL || out_size == 0)
        return -1;
    out[0] = '\0';

    if (libcmd_find_exec("oh-my-posh", libcmd_getenv("PATH"),
                         exec_path, sizeof(exec_path)) < 0)
        return -1;

    if (ctx->omp_line_start_ms > 0)
    {
        long long now = cmd_starship_monotonic_ms();
        if (now > ctx->omp_line_start_ms)
            dur = now - ctx->omp_line_start_ms;
    }

    libcmd_get_terminal_size(&cols, NULL);

    config = ctx->omp_config;
    if (config == NULL || config[0] == '\0')
        config = libcmd_getenv("POSH_THEME");

    /* First attempt: full flag set (current oh-my-posh). */
    {
        int n = libcmd_sprintf_s(cmd, sizeof(cmd),
                                 "%s print primary --status=%d"
                                 " --execution-time=%lld"
                                 " --terminal-width=%d --escape=false",
                                 exec_path, ctx->exit_code, dur, cols);
        if (n < 0)
            return -1;
    }
    if (config != NULL && config[0] != '\0')
    {
        size_t clen = strlen(cmd);
        libcmd_sprintf_s(cmd + clen, sizeof(cmd) - clen,
                         " --config=\"%s\"", config);
    }

    fp = libcmd_popen(cmd, "r");
    if (fp == NULL)
        return -1;

    while (total < out_size - 1)
    {
        size_t r = fread(out + total, 1, out_size - 1 - total, fp);
        if (r == 0)
            break;
        total += r;
    }
    rc = libcmd_pclose(fp);

    /* Older oh-my-posh rejects unknown flags; retry with a minimal set. */
    if (rc != 0 || total == 0)
    {
        total = 0;
        {
            int n = libcmd_sprintf_s(cmd, sizeof(cmd),
                                     "%s print primary --status=%d"
                                     " --execution-time=%lld",
                                     exec_path, ctx->exit_code, dur);
            if (n < 0)
                return -1;
        }
        if (config != NULL && config[0] != '\0')
        {
            size_t clen = strlen(cmd);
            libcmd_sprintf_s(cmd + clen, sizeof(cmd) - clen,
                             " --config=\"%s\"", config);
        }
        fp = libcmd_popen(cmd, "r");
        if (fp == NULL)
            return -1;
        while (total < out_size - 1)
        {
            size_t r = fread(out + total, 1, out_size - 1 - total, fp);
            if (r == 0)
                break;
            total += r;
        }
        libcmd_pclose(fp);
        if (total == 0)
            return -1;
    }

    out[total] = '\0';
    return (int)total;
}

/* -------------------------------------------------------------------------
 * Builtin
 * ---------------------------------------------------------------------- */

int builtin_omp(cmd_context_t *ctx, int argc, char **argv)
{
    if (argc >= 2 && libcmd_strcasecmp(argv[1], "/?") == 0)
    {
        fputs(cmd_gettext(MSG_HELP_OMP), stdout);
        return 0;
    }

    if (argc <= 1)
    {
        /* Report current state */
        fputs("oh-my-posh prompt is ", stdout);
        fputs(cmd_gettext(ctx->omp ? MSG_ON : MSG_OFF), stdout);
        fputc('\n', stdout);
        return 0;
    }

    if (libcmd_strcasecmp(argv[1], "init") == 0)
    {
        char exec_path[CMD_MAX_PATH];

        if (libcmd_find_exec("oh-my-posh", libcmd_getenv("PATH"),
                             exec_path, sizeof(exec_path)) < 0)
        {
            fputs(cmd_gettext(MSG_ERR_OMP_NOT_FOUND), stderr);
            return 1;
        }

        if (ctx->omp_config)
        {
            free(ctx->omp_config);
            ctx->omp_config = NULL;
        }
        if (argc >= 3 && argv[2][0])
        {
            ctx->omp_config = strdup(argv[2]);
            libcmd_setenv("POSH_THEME", argv[2], 1);
        }
        libcmd_setenv("POSH_SHELL", "cmd", 1);
        ctx->starship = 0;  /* oh-my-posh and starship are mutually exclusive */
        ctx->omp = 1;
        ctx->omp_line_start_ms = 0;
        return 0;
    }

    if (libcmd_strcasecmp(argv[1], "off") == 0 ||
        libcmd_strcasecmp(argv[1], "deinit") == 0 ||
        libcmd_strcasecmp(argv[1], "disable") == 0)
    {
        ctx->omp = 0;
        if (ctx->omp_config)
        {
            free(ctx->omp_config);
            ctx->omp_config = NULL;
        }
        return 0;
    }

    /* Any other subcommand (config, prompt, get, explain, ...) is
     * delegated to the real oh-my-posh binary. */
    {
        char exec_path[CMD_MAX_PATH];
        libcmd_exit_info_t exit_info;

        if (libcmd_find_exec("oh-my-posh", libcmd_getenv("PATH"),
                             exec_path, sizeof(exec_path)) < 0)
        {
            fputs(cmd_gettext(MSG_ERR_OMP_NOT_FOUND), stderr);
            return 1;
        }
        if (libcmd_exec_sync(exec_path, argv, libcmd_get_environ(),
                             -1, -1, -1, 0, &exit_info) < 0)
            return 1;
        return exit_info.exit_code;
    }
}