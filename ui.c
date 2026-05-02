#define _POSIX_C_SOURCE 200809L

#include "ui.h"

#include <ncurses.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <wchar.h>
#include <locale.h>

/* ---------- color setup -------------------------------------------------- */

/*
 * We build a 12-stop rainbow palette for the header bar by allocating one
 * color pair per cell and initializing each pair against terminal-default
 * background. On terminals with <256 colors we fall back to the 8 base
 * ANSI colors and just cycle them.
 */
#define HEADER_PAIRS_BASE 100   /* color-pair ids 100..111 */
#define HEADER_PAIRS_N    12

#define PAIR_BORDER       1
#define PAIR_BORDER_DIM   2
#define PAIR_LABEL        3
#define PAIR_SELF         4
#define PAIR_PEER         5
#define PAIR_SYS          6
#define PAIR_ERR          7
#define PAIR_INPUT        8
#define PAIR_STATUS       9
#define PAIR_FOOTER      10
#define PAIR_KEYHINT     11

static int g_have_256;

static void init_colors(void) {
    start_color();
    use_default_colors();
    g_have_256 = (COLORS >= 256);

    if (g_have_256) {
        /* btop-ish accents: cyan border, magenta labels, green/cyan chat. */
        init_pair(PAIR_BORDER,     51,  -1);   /* bright cyan        */
        init_pair(PAIR_BORDER_DIM, 24,  -1);   /* dim cyan           */
        init_pair(PAIR_LABEL,     201,  -1);   /* magenta            */
        init_pair(PAIR_SELF,       46,  -1);   /* bright green       */
        init_pair(PAIR_PEER,       45,  -1);   /* cyan               */
        init_pair(PAIR_SYS,       244,  -1);   /* gray               */
        init_pair(PAIR_ERR,       196,  -1);   /* red                */
        init_pair(PAIR_INPUT,     228,  -1);   /* pale yellow        */
        init_pair(PAIR_STATUS,     46,  -1);   /* green dot          */
        init_pair(PAIR_FOOTER,    240,  -1);
        init_pair(PAIR_KEYHINT,   213,  -1);   /* pink               */

        /* 12-stop rainbow for the header bar. */
        static const short stops[HEADER_PAIRS_N] = {
            196, 202, 208, 220, 226, 154,
             46,  49,  51,  39,  93, 201,
        };
        for (int i = 0; i < HEADER_PAIRS_N; ++i)
            init_pair((short)(HEADER_PAIRS_BASE + i), stops[i], -1);
    } else {
        init_pair(PAIR_BORDER,     COLOR_CYAN,    -1);
        init_pair(PAIR_BORDER_DIM, COLOR_BLUE,    -1);
        init_pair(PAIR_LABEL,      COLOR_MAGENTA, -1);
        init_pair(PAIR_SELF,       COLOR_GREEN,   -1);
        init_pair(PAIR_PEER,       COLOR_CYAN,    -1);
        init_pair(PAIR_SYS,        COLOR_WHITE,   -1);
        init_pair(PAIR_ERR,        COLOR_RED,     -1);
        init_pair(PAIR_INPUT,      COLOR_YELLOW,  -1);
        init_pair(PAIR_STATUS,     COLOR_GREEN,   -1);
        init_pair(PAIR_FOOTER,     COLOR_WHITE,   -1);
        init_pair(PAIR_KEYHINT,    COLOR_MAGENTA, -1);

        static const short stops[8] = {
            COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN,
            COLOR_BLUE, COLOR_MAGENTA, COLOR_RED, COLOR_YELLOW,
        };
        for (int i = 0; i < HEADER_PAIRS_N; ++i)
            init_pair((short)(HEADER_PAIRS_BASE + i),
                      stops[i % 8], -1);
    }
}

/* ---------- message scrollback ------------------------------------------ */

#define MSG_CAP 1024

typedef struct {
    ui_msg_kind kind;
    char        ts[12];
    char       *sender;   /* malloc'd, may be NULL */
    char       *text;     /* malloc'd               */
} ui_msg;

static ui_msg     g_msgs[MSG_CAP];
static size_t     g_msg_head;     /* next write slot      */
static size_t     g_msg_count;    /* up to MSG_CAP        */
static size_t     g_scroll_off;   /* lines from bottom    */
static pthread_mutex_t g_msg_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- stats -------------------------------------------------------- */

static _Atomic size_t g_sent_msgs;
static _Atomic size_t g_recv_msgs;
static _Atomic size_t g_sent_bytes;
static _Atomic size_t g_recv_bytes;
static _Atomic long   g_last_recv_unix;

/* ---------- ncurses windows --------------------------------------------- */

static WINDOW *w_header;
static WINDOW *w_side;
static WINDOW *w_chat;
static WINDOW *w_input;
static WINDOW *w_footer;

static const char *g_dest_ip;
static char        g_key_fp[24];   /* "ab:cd:ef:gh"        */

/* terminal dims captured on init/resize */
static int g_rows, g_cols;
static int g_side_w;     /* sidebar width                  */
static int g_input_h = 3;
static int g_header_h = 3;
static int g_footer_h = 1;

/* ---------- helpers ----------------------------------------------------- */

static void fmt_bytes(size_t n, char *out, size_t outsz) {
    if (n < 1024) {
        snprintf(out, outsz, "%zuB", n);
    } else if (n < 1024UL * 1024UL) {
        snprintf(out, outsz, "%.1fK", n / 1024.0);
    } else if (n < 1024UL * 1024UL * 1024UL) {
        snprintf(out, outsz, "%.2fM", n / (1024.0 * 1024.0));
    } else {
        snprintf(out, outsz, "%.2fG", n / (1024.0 * 1024.0 * 1024.0));
    }
}

static void timestamp_now(char *out, size_t outsz) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(out, outsz, "%H:%M:%S", &tm);
}

/* ---------- drawing ----------------------------------------------------- */

static void draw_box(WINDOW *w, int pair, const char *title) {
    wattr_set(w, A_NORMAL, (short)pair, NULL);
    /* Use rounded-ish unicode box. ACS_* gives portable line art. */
    wborder(w,
            ACS_VLINE,    ACS_VLINE,
            ACS_HLINE,    ACS_HLINE,
            ACS_ULCORNER, ACS_URCORNER,
            ACS_LLCORNER, ACS_LRCORNER);
    if (title && *title) {
        wmove(w, 0, 2);
        waddch(w, ' ');
        wattr_set(w, A_BOLD, PAIR_LABEL, NULL);
        waddstr(w, title);
        wattr_set(w, A_NORMAL, (short)pair, NULL);
        waddch(w, ' ');
    }
}

static void draw_header(void) {
    werase(w_header);
    /* Top/bottom border for the header band itself */
    wattr_set(w_header, A_NORMAL, PAIR_BORDER, NULL);
    mvwhline(w_header, 0, 0, ACS_HLINE, g_cols);
    mvwhline(w_header, 2, 0, ACS_HLINE, g_cols);
    mvwaddch(w_header, 0, 0, ACS_ULCORNER);
    mvwaddch(w_header, 0, g_cols - 1, ACS_URCORNER);
    mvwaddch(w_header, 2, 0, ACS_LTEE);
    mvwaddch(w_header, 2, g_cols - 1, ACS_RTEE);
    mvwaddch(w_header, 1, 0, ACS_VLINE);
    mvwaddch(w_header, 1, g_cols - 1, ACS_VLINE);

    /* Rainbow title centered on row 1. */
    static const char *title = " ping-chat :: ICMP encrypted messenger ";
    int tlen = (int)strlen(title);
    int start = (g_cols - tlen) / 2;
    if (start < 2) start = 2;

    for (int i = 0; i < tlen && start + i < g_cols - 1; ++i) {
        int slot = (i * HEADER_PAIRS_N) / (tlen ? tlen : 1);
        if (slot >= HEADER_PAIRS_N) slot = HEADER_PAIRS_N - 1;
        wattr_set(w_header, A_BOLD, (short)(HEADER_PAIRS_BASE + slot), NULL);
        mvwaddch(w_header, 1, start + i, (chtype)title[i]);
    }

    /* Right-side clock */
    char ts[16];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%H:%M:%S", &tm);
    int tx = g_cols - (int)strlen(ts) - 3;
    if (tx > start + tlen + 2) {
        wattr_set(w_header, A_DIM, PAIR_LABEL, NULL);
        mvwaddstr(w_header, 1, tx, ts);
    }

    wnoutrefresh(w_header);
}

static void draw_sidebar(void) {
    werase(w_side);
    draw_box(w_side, PAIR_BORDER, "peer");

    int row = 2;
    int innerw = g_side_w - 4;
    if (innerw < 8) innerw = 8;

    /* target IP */
    wattr_set(w_side, A_DIM, PAIR_LABEL, NULL);
    mvwaddstr(w_side, row, 2, "target");
    wattr_set(w_side, A_BOLD, PAIR_PEER, NULL);
    mvwaddnstr(w_side, row + 1, 2, g_dest_ip, innerw);
    row += 3;

    /* status dot */
    long last = atomic_load(&g_last_recv_unix);
    long now  = (long)time(NULL);
    int alive = (last != 0) && (now - last < 30);
    wattr_set(w_side, A_BOLD, alive ? PAIR_STATUS : PAIR_SYS, NULL);
    mvwaddstr(w_side, row, 2, alive ? "● online" : "○ idle");
    row += 2;

    /* stats sub-box */
    int stats_top = row;
    int stats_h   = 7;
    if (stats_top + stats_h < getmaxy(w_side) - 4) {
        wattr_set(w_side, A_NORMAL, PAIR_BORDER_DIM, NULL);
        mvwaddch(w_side, stats_top, 1, ACS_ULCORNER);
        mvwhline(w_side, stats_top, 2, ACS_HLINE, g_side_w - 4);
        mvwaddch(w_side, stats_top, g_side_w - 2, ACS_URCORNER);
        for (int i = 1; i < stats_h - 1; ++i) {
            mvwaddch(w_side, stats_top + i, 1, ACS_VLINE);
            mvwaddch(w_side, stats_top + i, g_side_w - 2, ACS_VLINE);
        }
        mvwaddch(w_side, stats_top + stats_h - 1, 1, ACS_LLCORNER);
        mvwhline(w_side, stats_top + stats_h - 1, 2, ACS_HLINE, g_side_w - 4);
        mvwaddch(w_side, stats_top + stats_h - 1, g_side_w - 2, ACS_LRCORNER);

        wattr_set(w_side, A_BOLD, PAIR_LABEL, NULL);
        mvwaddstr(w_side, stats_top, 3, " stats ");

        size_t sm = atomic_load(&g_sent_msgs);
        size_t rm = atomic_load(&g_recv_msgs);
        size_t sb = atomic_load(&g_sent_bytes);
        size_t rb = atomic_load(&g_recv_bytes);
        char sbuf[16], rbuf[16];
        fmt_bytes(sb, sbuf, sizeof(sbuf));
        fmt_bytes(rb, rbuf, sizeof(rbuf));

        wattr_set(w_side, A_DIM, PAIR_LABEL, NULL);
        mvwaddstr(w_side, stats_top + 1, 3, "sent");
        wattr_set(w_side, A_BOLD, PAIR_SELF, NULL);
        mvwprintw(w_side, stats_top + 1, g_side_w - 3 - 6, "%6zu", sm);

        wattr_set(w_side, A_DIM, PAIR_LABEL, NULL);
        mvwaddstr(w_side, stats_top + 2, 3, "recv");
        wattr_set(w_side, A_BOLD, PAIR_PEER, NULL);
        mvwprintw(w_side, stats_top + 2, g_side_w - 3 - 6, "%6zu", rm);

        wattr_set(w_side, A_DIM, PAIR_LABEL, NULL);
        mvwaddstr(w_side, stats_top + 3, 3, "tx");
        wattr_set(w_side, A_BOLD, PAIR_SELF, NULL);
        mvwprintw(w_side, stats_top + 3, g_side_w - 3 - 6, "%6s", sbuf);

        wattr_set(w_side, A_DIM, PAIR_LABEL, NULL);
        mvwaddstr(w_side, stats_top + 4, 3, "rx");
        wattr_set(w_side, A_BOLD, PAIR_PEER, NULL);
        mvwprintw(w_side, stats_top + 4, g_side_w - 3 - 6, "%6s", rbuf);

        row = stats_top + stats_h + 1;
    }

    /* key fingerprint */
    if (row + 2 < getmaxy(w_side) - 1) {
        wattr_set(w_side, A_DIM, PAIR_LABEL, NULL);
        mvwaddstr(w_side, row, 2, "key");
        wattr_set(w_side, A_NORMAL, PAIR_KEYHINT, NULL);
        mvwaddnstr(w_side, row + 1, 2, g_key_fp, innerw);
    }

    wnoutrefresh(w_side);
}

/* word-wrap a single message into the chat window starting at row `*y`,
 * descending. We render bottom-up so we walk lines in reverse. Returns
 * number of physical rows the message consumed.
 */
static int render_msg_into_buf(const ui_msg *m, int innerw, char ***out_lines) {
    /* Build the prefix:  "[hh:mm:ss] sender: " */
    char prefix[64];
    if (m->kind == UI_MSG_SYS || m->kind == UI_MSG_ERR) {
        snprintf(prefix, sizeof(prefix), "[%s] * ", m->ts);
    } else if (m->sender) {
        snprintf(prefix, sizeof(prefix), "[%s] %s: ", m->ts, m->sender);
    } else {
        snprintf(prefix, sizeof(prefix), "[%s] ", m->ts);
    }

    size_t plen = strlen(prefix);
    size_t tlen = strlen(m->text);
    size_t full_len = plen + tlen + 1;
    char *full = malloc(full_len);
    if (!full) { *out_lines = NULL; return 0; }
    memcpy(full, prefix, plen);
    memcpy(full + plen, m->text, tlen + 1);

    /* Wrap into lines of at most innerw columns. */
    int cap = 8, cnt = 0;
    char **lines = malloc((size_t)cap * sizeof(char *));
    size_t pos = 0, total = plen + tlen;
    while (pos < total) {
        size_t take = total - pos;
        if ((int)take > innerw) take = (size_t)innerw;
        /* on continuation lines, indent under the prefix */
        if (pos > 0) {
            size_t indent = (plen < (size_t)innerw / 2) ? plen : 2;
            char *line = malloc(indent + take + 1);
            memset(line, ' ', indent);
            memcpy(line + indent, full + pos, take);
            line[indent + take] = '\0';
            if (cnt == cap) {
                cap *= 2;
                lines = realloc(lines, (size_t)cap * sizeof(char *));
            }
            lines[cnt++] = line;
        } else {
            char *line = malloc(take + 1);
            memcpy(line, full + pos, take);
            line[take] = '\0';
            if (cnt == cap) {
                cap *= 2;
                lines = realloc(lines, (size_t)cap * sizeof(char *));
            }
            lines[cnt++] = line;
        }
        pos += take;
    }
    if (cnt == 0) {
        lines[cnt++] = strdup("");
    }
    free(full);
    *out_lines = lines;
    return cnt;
}

static int kind_pair(ui_msg_kind k) {
    switch (k) {
        case UI_MSG_SELF: return PAIR_SELF;
        case UI_MSG_PEER: return PAIR_PEER;
        case UI_MSG_ERR:  return PAIR_ERR;
        default:          return PAIR_SYS;
    }
}

static void draw_chat(void) {
    werase(w_chat);
    draw_box(w_chat, PAIR_BORDER, "messages");

    int innerw = getmaxx(w_chat) - 4;
    int innerh = getmaxy(w_chat) - 2;
    if (innerw < 4 || innerh < 1) { wnoutrefresh(w_chat); return; }

    pthread_mutex_lock(&g_msg_lock);

    /* Walk newest -> oldest, rendering bottom-up. */
    int rows_left = innerh;
    int y = innerh; /* exclusive */
    size_t skipped = 0;

    /* iterate newest first */
    for (size_t i = 0; i < g_msg_count && rows_left > 0; ++i) {
        size_t idx = (g_msg_head + MSG_CAP - 1 - i) % MSG_CAP;
        const ui_msg *m = &g_msgs[idx];

        char **lines;
        int n = render_msg_into_buf(m, innerw, &lines);
        if (n == 0) continue;

        /* Honor scrollback offset by skipping the most-recent lines first. */
        int start_line = n; /* exclusive, will paint up to this */
        if (skipped < g_scroll_off) {
            size_t need = g_scroll_off - skipped;
            if ((size_t)n <= need) {
                skipped += (size_t)n;
                for (int k = 0; k < n; ++k) free(lines[k]);
                free(lines);
                continue;
            }
            start_line = n - (int)need;
            skipped = g_scroll_off;
        }

        int paint = start_line;
        if (paint > rows_left) {
            int drop = paint - rows_left;
            /* paint the bottom rows_left lines */
            for (int k = paint - rows_left; k < paint; ++k) {
                y--;
                wattr_set(w_chat, A_NORMAL, kind_pair(m->kind), NULL);
                mvwaddnstr(w_chat, y + 1, 2, lines[k], innerw);
            }
            (void)drop;
            rows_left = 0;
        } else {
            for (int k = paint - 1; k >= 0; --k) {
                y--;
                wattr_set(w_chat, A_NORMAL, kind_pair(m->kind), NULL);
                mvwaddnstr(w_chat, y + 1, 2, lines[k], innerw);
                rows_left--;
                if (rows_left == 0) break;
            }
        }

        for (int k = 0; k < n; ++k) free(lines[k]);
        free(lines);
    }

    /* scrollback indicator */
    if (g_scroll_off > 0) {
        wattr_set(w_chat, A_BOLD, PAIR_KEYHINT, NULL);
        mvwprintw(w_chat, 0, getmaxx(w_chat) - 18, " scroll +%zu ",
                  g_scroll_off);
    }

    pthread_mutex_unlock(&g_msg_lock);
    wnoutrefresh(w_chat);
}

static void draw_input(const char *buf, int cursor) {
    werase(w_input);
    draw_box(w_input, PAIR_BORDER, "input");

    int innerw = getmaxx(w_input) - 6;
    if (innerw < 4) innerw = 4;

    /* horizontal scroll so cursor stays visible */
    int len = (int)strlen(buf);
    int view_off = 0;
    if (cursor > innerw - 1) view_off = cursor - (innerw - 1);
    if (view_off > len) view_off = len;

    wattr_set(w_input, A_BOLD, PAIR_KEYHINT, NULL);
    mvwaddstr(w_input, 1, 2, "›");
    wattr_set(w_input, A_NORMAL, PAIR_INPUT, NULL);
    mvwaddnstr(w_input, 1, 4, buf + view_off, innerw);

    /* place cursor */
    int cx = 4 + (cursor - view_off);
    if (cx > getmaxx(w_input) - 2) cx = getmaxx(w_input) - 2;
    wmove(w_input, 1, cx);

    wnoutrefresh(w_input);
}

static void draw_footer(void) {
    werase(w_footer);
    wattr_set(w_footer, A_DIM, PAIR_FOOTER, NULL);
    mvwaddstr(w_footer, 0, 1,
              " enter send · pgup/pgdn scroll · ctrl-c quit ");
    wnoutrefresh(w_footer);
}

/* ---------- layout ------------------------------------------------------ */

static void destroy_windows(void) {
    if (w_header) { delwin(w_header); w_header = NULL; }
    if (w_side)   { delwin(w_side);   w_side   = NULL; }
    if (w_chat)   { delwin(w_chat);   w_chat   = NULL; }
    if (w_input)  { delwin(w_input);  w_input  = NULL; }
    if (w_footer) { delwin(w_footer); w_footer = NULL; }
}

static void layout(void) {
    getmaxyx(stdscr, g_rows, g_cols);
    if (g_rows < 12) g_rows = 12;
    if (g_cols < 50) g_cols = 50;

    g_side_w = g_cols / 4;
    if (g_side_w < 22) g_side_w = 22;
    if (g_side_w > 32) g_side_w = 32;

    int body_top = g_header_h;
    int body_h   = g_rows - g_header_h - g_input_h - g_footer_h;
    if (body_h < 4) body_h = 4;

    destroy_windows();

    w_header = newwin(g_header_h, g_cols, 0, 0);
    w_side   = newwin(body_h, g_side_w, body_top, 0);
    w_chat   = newwin(body_h, g_cols - g_side_w, body_top, g_side_w);
    w_input  = newwin(g_input_h, g_cols, body_top + body_h, 0);
    w_footer = newwin(g_footer_h, g_cols, g_rows - g_footer_h, 0);
}

/* ---------- public API -------------------------------------------------- */

static volatile sig_atomic_t g_resized;
static void on_winch(int sig) { (void)sig; g_resized = 1; }

void ui_init(const char *dest_ip, const uint8_t key[32]) {
    g_dest_ip = dest_ip;
    snprintf(g_key_fp, sizeof(g_key_fp),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             key[0], key[1], key[2], key[3], key[4], key[5]);

    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    curs_set(1);
    set_escdelay(25);
    init_colors();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_winch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    layout();

    /* initial paint */
    erase();
    wnoutrefresh(stdscr);
    draw_header();
    draw_sidebar();
    draw_chat();
    draw_footer();
    doupdate();

    ui_push_message(UI_MSG_SYS, NULL, "ping-chat ready. type to send.");
}

void ui_shutdown(void) {
    destroy_windows();
    endwin();

    pthread_mutex_lock(&g_msg_lock);
    for (size_t i = 0; i < g_msg_count; ++i) {
        size_t idx = (g_msg_head + MSG_CAP - g_msg_count + i) % MSG_CAP;
        free(g_msgs[idx].sender);
        free(g_msgs[idx].text);
    }
    g_msg_count = 0;
    g_msg_head  = 0;
    pthread_mutex_unlock(&g_msg_lock);
}

void ui_push_message(ui_msg_kind kind, const char *sender, const char *text) {
    if (!text) text = "";

    pthread_mutex_lock(&g_msg_lock);
    ui_msg *slot = &g_msgs[g_msg_head];
    if (g_msg_count == MSG_CAP) {
        free(slot->sender);
        free(slot->text);
    }
    slot->kind = kind;
    timestamp_now(slot->ts, sizeof(slot->ts));
    slot->sender = sender ? strdup(sender) : NULL;
    slot->text   = strdup(text);
    g_msg_head = (g_msg_head + 1) % MSG_CAP;
    if (g_msg_count < MSG_CAP) g_msg_count++;
    pthread_mutex_unlock(&g_msg_lock);
}

void ui_stat_sent(size_t bytes) {
    atomic_fetch_add(&g_sent_msgs, 1);
    atomic_fetch_add(&g_sent_bytes, bytes);
}

void ui_stat_recv(size_t bytes) {
    atomic_fetch_add(&g_recv_msgs, 1);
    atomic_fetch_add(&g_recv_bytes, bytes);
    atomic_store(&g_last_recv_unix, (long)time(NULL));
}

/* ---------- input loop -------------------------------------------------- */

#define INPUT_MAX 4096

char *ui_read_line(void) {
    static char buf[INPUT_MAX];
    int len = 0, cursor = 0;
    buf[0] = '\0';

    /* refresh whole UI initially */
    draw_header();
    draw_sidebar();
    draw_chat();
    draw_input(buf, cursor);
    draw_footer();
    doupdate();

    /* poll w/ short timeout so we can repaint stats / incoming msgs */
    wtimeout(stdscr, 120);

    for (;;) {
        if (g_resized) {
            g_resized = 0;
            endwin();
            refresh();
            layout();
        }

        int ch = wgetch(stdscr);

        /* periodic repaint of dynamic regions (clock, stats, chat) */
        draw_header();
        draw_sidebar();
        draw_chat();

        if (ch == ERR) {
            draw_input(buf, cursor);
            doupdate();
            continue;
        }

        if (ch == KEY_RESIZE) {
            layout();
            continue;
        }

        if (ch == 3 /* ^C */ || ch == 4 /* ^D on empty */) {
            if (ch == 4 && len > 0) { /* ignore ^D mid-line */ }
            else return NULL;
        }

        if (ch == '\r' || ch == '\n' || ch == KEY_ENTER) {
            if (len == 0) {
                draw_input(buf, cursor);
                doupdate();
                continue;
            }
            if (strcmp(buf, "QUIT") == 0 || strcmp(buf, "/quit") == 0)
                return NULL;
            char *out = strndup(buf, (size_t)len);
            len = 0; cursor = 0; buf[0] = '\0';
            return out;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (cursor > 0) {
                memmove(buf + cursor - 1, buf + cursor,
                        (size_t)(len - cursor + 1));
                cursor--; len--;
            }
        } else if (ch == KEY_DC) {
            if (cursor < len) {
                memmove(buf + cursor, buf + cursor + 1,
                        (size_t)(len - cursor));
                len--;
            }
        } else if (ch == KEY_LEFT) {
            if (cursor > 0) cursor--;
        } else if (ch == KEY_RIGHT) {
            if (cursor < len) cursor++;
        } else if (ch == KEY_HOME || ch == 1 /* ^A */) {
            cursor = 0;
        } else if (ch == KEY_END || ch == 5 /* ^E */) {
            cursor = len;
        } else if (ch == 21 /* ^U */) {
            len = 0; cursor = 0; buf[0] = '\0';
        } else if (ch == 11 /* ^K */) {
            buf[cursor] = '\0';
            len = cursor;
        } else if (ch == 23 /* ^W */) {
            int e = cursor;
            while (e > 0 && isspace((unsigned char)buf[e - 1])) e--;
            while (e > 0 && !isspace((unsigned char)buf[e - 1])) e--;
            memmove(buf + e, buf + cursor, (size_t)(len - cursor + 1));
            len -= cursor - e;
            cursor = e;
        } else if (ch == KEY_PPAGE) {
            pthread_mutex_lock(&g_msg_lock);
            g_scroll_off += 5;
            if (g_scroll_off > g_msg_count * 4) g_scroll_off = g_msg_count * 4;
            pthread_mutex_unlock(&g_msg_lock);
        } else if (ch == KEY_NPAGE) {
            pthread_mutex_lock(&g_msg_lock);
            if (g_scroll_off >= 5) g_scroll_off -= 5;
            else g_scroll_off = 0;
            pthread_mutex_unlock(&g_msg_lock);
        } else if (ch >= 32 && ch < 127) {
            if (len + 1 < INPUT_MAX - 1) {
                memmove(buf + cursor + 1, buf + cursor,
                        (size_t)(len - cursor + 1));
                buf[cursor] = (char)ch;
                cursor++; len++;
            }
        }
        /* ignore everything else */

        draw_input(buf, cursor);
        doupdate();
    }
}
