// clink_input.cpp - C bridge exposing the Clink Unix terminal input backend
// (clink-posix unix_terminal_in) to the C command interpreter.
//
// When the command shell is built with -DUSE_CLINK_INPUT, llinenoise.c routes
// its raw-mode setup and byte reads through these functions, so line editing
// runs on the Clink terminal input layer (termios raw mode, poll-based reads,
// SIGWINCH resize detection) instead of linenoise's own termios handling.
//
// License: GPL-3.0-or-later

#include "unix_terminal_in.h"

extern "C" {

static unix_terminal_in* s_ti = 0;

int clink_input_begin(void)
{
    if (s_ti == 0)
        s_ti = new unix_terminal_in(true);
    return s_ti->begin(true) > 0 ? 0 : -1;
}

void clink_input_end(void)
{
    if (s_ti != 0)
        s_ti->end(true);
}

/* Returns a raw byte (0..255) or one of the special input_* values
 * (e.g. input_terminal_resize), or -1 when the backend is not running. */
int clink_input_read(void)
{
    if (s_ti == 0)
        return -1;
    return (int)s_ti->read();
}

} // extern "C"