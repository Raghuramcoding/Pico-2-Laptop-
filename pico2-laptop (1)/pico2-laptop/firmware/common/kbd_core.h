/* Keyboard logic that does not touch hardware: debounce, key map, shift,
 * caps, ctrl, auto-repeat.  The BIOS feeds it raw matrix scans. */
#ifndef KBD_CORE_H
#define KBD_CORE_H
#include <stdint.h>

#define KB_ROWS 8
#define KB_COLS 9

enum {
    K_BKSP = 8, K_TAB = 9, K_ENTER = 13, K_ESC = 27, K_DEL = 127,
    K_UP = 0x80, K_DOWN, K_LEFT, K_RIGHT,
    K_LSHIFT = 0x90, K_RSHIFT, K_CTRL, K_ALT, K_FN, K_CAPS
};

extern const uint8_t kb_keymap[KB_ROWS][KB_COLS];

void kbd_core_init(void);
/* raw[r] has bit c set while the key at row r, column c is down */
void kbd_core_feed(const uint16_t raw[KB_ROWS], uint32_t now_ms);
int  kbd_core_getc(void);        /* next key code, or -1 */
#endif
