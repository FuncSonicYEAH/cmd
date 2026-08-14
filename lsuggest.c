/*
 * lsuggest.c - interactive command-line hints for cmd
 *
 * Two opt-in editing features, enabled through environment variables
 * (the "suggest" omuc plugin sets them; you may also set them by hand):
 *
 *   OMUC_SUGGEST=on   fish-style history suggestions.  The most recent
 *                     history entry that starts with the current line is
 *                     shown dimmed right after the cursor; press Right or
 *                     Ctrl+F to accept it.
 *   OMUC_CHECK=on     paint the first token of the edited line green when
 *                     it names an existing command (builtin, external on
 *                     PATH, or an accessible file) and red otherwise.
 *
 * Both are read on every line refresh, so toggling the environment
 * variables takes effect immediately without restarting the session.
 *
 * License: GNU GPLv3
 */

#include "glibcmd.h"
#include "ccontext.h"
#include "glinenoise.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ANSI SGR colours used for the features above. */
#define OMUC_HINT_COLOR 90 /* bright black / grey */
#define OMUC_OK_COLOR   32 /* green: the command exists */
#define OMUC_BAD_COLOR  31 /* red:   the command was not found */

/* -------------------------------------------------------------------------
 * Feature toggles
 * ---------------------------------------------------------------------- */

static int env_is_on(const char *name)
{
    const char *v = libcmd_getenv(name);
    return v != NULL && libcmd_strcasecmp(v, "on") == 0;
}

static int suggest_enabled(void) { return env_is_on("OMUC_SUGGEST"); }
static int check_enabled(void)   { return env_is_on("OMUC_CHECK"); }

/* -------------------------------------------------------------------------
 * History suggestion (hints callback)
 * ---------------------------------------------------------------------- */

/* Find the most recent history entry that strictly starts with 'line' and
 * return a heap-allocated copy of the part that follows it (the hint that
 * gets appended after the cursor), or NULL when there is nothing to
 * suggest.  The newest entry is the current editing buffer and is skipped
 * so a finished command is never suggested while retyping it. */
static char *suggest_find_hint(const char *line)
{
    size_t plen = strlen(line);
    int n = linenoiseHistoryLength();
    int i;

    for (i = n - 1; i >= 0; i--) {
        const char *h = linenoiseHistoryGet(i);
        if (h != NULL && h[0] != '\0' &&
            strlen(h) > plen &&
            libcmd_strncasecmp(h, line, plen) == 0)
        {
            return strdup(h + plen);
        }
    }
    return NULL;
}

static char *suggest_hints_callback(const char *line, int *color, int *bold)
{
    char *hint;

    if (!suggest_enabled() || line == NULL || line[0] == '\0')
        return NULL;
    hint = suggest_find_hint(line);
    if (hint == NULL)
        return NULL;
    *color = OMUC_HINT_COLOR;
    *bold  = 0;
    return hint;
}

static void suggest_free_callback(void *ptr)
{
    free(ptr);
}

/* -------------------------------------------------------------------------
 * Command existence colour (command-colour callback)
 * ---------------------------------------------------------------------- */

static int command_exists(const char *token)
{
    char path[CMD_MAX_PATH];
    const char *path_env;
    const char *ext;

    if (token[0] == '\0')
        return 0;

    /* Internal commands. */
    if (cmd_find_builtin(token) != NULL)
        return 1;

    /* Paths the user wrote directly (containing a separator). */
    if (libcmd_path_is_abs(token) ||
        strchr(token, '/') != NULL || strchr(token, '\\') != NULL)
    {
        char norm[CMD_MAX_PATH];
        libcmd_sprintf_s(norm, sizeof(norm), "%s", token);
        libcmd_path_norm_sep(norm);
        return libcmd_access(norm, 1) == 0;
    }

    /* External programs on PATH. */
    path_env = libcmd_getenv("PATH");
    if (libcmd_find_exec(token, path_env, path, sizeof(path)) == 0)
        return 1;

    /* Batch files in the current directory run when named with an
     * extension (e.g. build.bat), like the dispatcher does. */
    ext = libcmd_path_ext(token);
    if (ext[0] != '\0' &&
        (libcmd_strcasecmp(ext, ".bat") == 0 ||
         libcmd_strcasecmp(ext, ".cmd") == 0))
    {
        if (libcmd_access(token, 0) == 0)
            return 1;
    }

    return 0;
}

static int suggest_cmd_color_callback(const char *buf, size_t len)
{
    char token[CMD_MAX_PATH];
    size_t i, tlen;

    if (!check_enabled() || len == 0)
        return 0;

    i = 0;
    while (i < len && buf[i] != ' ' && buf[i] != '\t')
        i++;
    tlen = i < sizeof(token) - 1 ? i : sizeof(token) - 1;
    if (tlen == 0)
        return 0;
    memcpy(token, buf, tlen);
    token[tlen] = '\0';

    return command_exists(token) ? OMUC_OK_COLOR : OMUC_BAD_COLOR;
}

/* -------------------------------------------------------------------------
 * Initialization
 * ---------------------------------------------------------------------- */

/* Install the editing callbacks.  Called from libcmd_readline_init().
 * The features stay inert until the corresponding environment variable
 * is turned on. */
void libcmd_suggest_init(void)
{
    linenoiseSetHintsCallback(suggest_hints_callback);
    linenoiseSetFreeHintsCallback(suggest_free_callback);
    linenoiseSetCmdColorCallback(suggest_cmd_color_callback);
}