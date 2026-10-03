#include "kbd_core.h"

/* Matrix layout (also printed in docs/WIRING.md).  Each keyboard row is split
 * over two matrix rows so 8 rows x 9 columns cover a 63-key board.
 * 0 = no key at that position. */
const uint8_t kb_keymap[KB_ROWS][KB_COLS] = {
    /*      c0   c1   c2   c3   c4   c5   c6   c7      c8        */
    /*r0*/ {'`', '1', '2', '3', '4', '5', '6', K_CTRL, K_ALT},
    /*r1*/ {'7', '8', '9', '0', '-', '=', K_BKSP, ' ', K_FN},
    /*r2*/ {K_TAB, 'q', 'w', 'e', 'r', 't', 'y', K_UP, K_DOWN},
    /*r3*/ {'u', 'i', 'o', 'p', '[', ']', '\\', K_LEFT, K_RIGHT},
    /*r4*/ {K_CAPS, 'a', 's', 'd', 'f', 'g', 'h', K_DEL, K_ESC},
    /*r5*/ {'j', 'k', 'l', ';', '\'', K_ENTER, 0, 0, 0},
    /*r6*/ {K_LSHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 0, 0},
    /*r7*/ {'m', ',', '.', '/', K_RSHIFT, 0, 0, 0, 0},
};

#define QSIZE 32
#define REPEAT_DELAY_MS 500
#define REPEAT_RATE_MS  40

static uint16_t stable[KB_ROWS], last_raw[KB_ROWS];
static uint8_t same_count;
static uint8_t shift_l, shift_r, ctrl, caps;
static int queue[QSIZE];
static uint8_t q_head, q_tail;
static int rep_key = -1;
static uint32_t rep_start, rep_last;

void kbd_core_init(void)
{
    for (int i = 0; i < KB_ROWS; i++) stable[i] = last_raw[i] = 0;
    same_count = shift_l = shift_r = ctrl = caps = 0;
    q_head = q_tail = 0;
    rep_key = -1;
}

static void push(int c)
{
    uint8_t n = (uint8_t)((q_head + 1) % QSIZE);
    if (n == q_tail) return;            /* full: drop */
    queue[q_head] = c;
    q_head = n;
}

static char shifted(char c)
{
    switch (c) {
    case '`': return '~'; case '1': return '!'; case '2': return '@';
    case '3': return '#'; case '4': return '$'; case '5': return '%';
    case '6': return '^'; case '7': return '&'; case '8': return '*';
    case '9': return '('; case '0': return ')'; case '-': return '_';
    case '=': return '+'; case '[': return '{'; case ']': return '}';
    case '\\': return '|'; case ';': return ':'; case '\'': return '"';
    case ',': return '<'; case '.': return '>'; case '/': return '?';
    default: return c;
    }
}

static int translate(uint8_t k)
{
    int sh = shift_l || shift_r;
    if (k >= 'a' && k <= 'z') {
        if (ctrl) return k - 'a' + 1;
        return (sh ^ caps) ? k - 'a' + 'A' : k;
    }
    if (k >= 0x80) return k;           /* arrows etc. */
    return sh ? shifted((char)k) : k;
}

static int is_modifier(uint8_t k)
{
    return k == K_LSHIFT || k == K_RSHIFT || k == K_CTRL || k == K_ALT || k == K_FN || k == K_CAPS;
}

static void press(uint8_t k, uint32_t now)
{
    switch (k) {
    case K_LSHIFT: shift_l = 1; return;
    case K_RSHIFT: shift_r = 1; return;
    case K_CTRL:   ctrl = 1; return;
    case K_ALT: case K_FN: return;
    case K_CAPS:   caps ^= 1; return;
    default: break;
    }
    int c = translate(k);
    push(c);
    rep_key = k;
    rep_start = rep_last = now;
}

static void release(uint8_t k)
{
    switch (k) {
    case K_LSHIFT: shift_l = 0; return;
    case K_RSHIFT: shift_r = 0; return;
    case K_CTRL:   ctrl = 0; return;
    default: break;
    }
    if (rep_key == k) rep_key = -1;
}

void kbd_core_feed(const uint16_t raw[KB_ROWS], uint32_t now_ms)
{
    int same = 1;
    for (int r = 0; r < KB_ROWS; r++) if (raw[r] != last_raw[r]) same = 0;
    if (same) { if (same_count < 255) same_count++; }
    else { same_count = 0; for (int r = 0; r < KB_ROWS; r++) last_raw[r] = raw[r]; }

    if (same_count >= 1) {             /* two scans in a row agree: debounced */
        /* pass 0: modifier presses, pass 1: normal keys, pass 2: modifier releases.
         * This way "Shift + a" pressed in the same scan still gives 'A'. */
        for (int pass = 0; pass < 3; pass++) {
            for (int r = 0; r < KB_ROWS; r++) {
                uint16_t diff = stable[r] ^ raw[r];
                for (int c = 0; c < KB_COLS; c++) {
                    if (!(diff & (1u << c))) continue;
                    uint8_t k = kb_keymap[r][c];
                    if (!k) continue;
                    int down = (raw[r] & (1u << c)) != 0;
                    int mod = is_modifier(k);
                    if (pass == 0 && mod && down) press(k, now_ms);
                    else if (pass == 1 && !mod) { if (down) press(k, now_ms); else release(k); }
                    else if (pass == 2 && mod && !down) release(k);
                }
            }
        }
        for (int r = 0; r < KB_ROWS; r++) stable[r] = raw[r];
    }

    if (rep_key >= 0 && (int32_t)(now_ms - rep_start) >= REPEAT_DELAY_MS &&
        (int32_t)(now_ms - rep_last) >= REPEAT_RATE_MS) {
        push(translate((uint8_t)rep_key));
        rep_last = now_ms;
    }
}

int kbd_core_getc(void)
{
    if (q_head == q_tail) return -1;
    int c = queue[q_tail];
    q_tail = (uint8_t)((q_tail + 1) % QSIZE);
    return c;
}
