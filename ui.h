#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stddef.h>

/* Message kinds — control how a line is colored in the chat pane. */
typedef enum {
    UI_MSG_SELF = 0,    /* messages we sent       */
    UI_MSG_PEER = 1,    /* messages we received   */
    UI_MSG_SYS  = 2,    /* system / status notes  */
    UI_MSG_ERR  = 3,    /* errors                 */
} ui_msg_kind;

/* Initialize the ncurses TUI.  Must be called once before any other ui_* call.
 * `dest_ip`   — peer address shown in the sidebar.
 * `key`       — 32-byte chat key, used to render a short fingerprint.
 */
void ui_init(const char *dest_ip, const uint8_t key[32]);

/* Tear down ncurses and restore the terminal. */
void ui_shutdown(void);

/* Append a message to the chat scrollback. Thread-safe. `sender` may be NULL
 * for system messages. The TUI will prepend a timestamp automatically. */
void ui_push_message(ui_msg_kind kind, const char *sender, const char *text);

/* Bookkeeping for the sidebar stat panel. Thread-safe. */
void ui_stat_sent(size_t bytes);
void ui_stat_recv(size_t bytes);

/* Block until the user submits a line of input or asks to quit.
 * On submit returns a malloc'd string the caller must free.
 * On quit (Ctrl-C / Ctrl-D / "QUIT") returns NULL. */
char *ui_read_line(void);

#endif
