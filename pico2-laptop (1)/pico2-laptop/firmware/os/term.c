/* 40x30 text terminal.  term_putc() only edits a character grid; term_flush()
 * (called by the "ui" task) redraws the rows that changed on the LCD. */
#include "os.h"
#include "bios_api.h"
#include <string.h>

#define TCOLS 40
#define TROWS 30
#define FG 0x87F0     /* light green, RGB565 */
#define BG 0x0000

static char grid[TROWS][TCOLS + 1];
static int cx, cy;
static uint32_t dirty;                /* one bit per row */
static int shown_row = -1, shown_col = -1;

static void mark_all(void) { dirty = (1u << TROWS) - 1u; }

void term_clear(void)
{
    for (int r = 0; r < TROWS; r++) { memset(grid[r], ' ', TCOLS); grid[r][TCOLS] = 0; }
    cx = cy = 0;
    mark_all();
}

void term_init(void) { term_clear(); }

static void newline(void)
{
    cx = 0;
    if (++cy >= TROWS) {
        memmove(grid[0], grid[1], (TROWS - 1) * (TCOLS + 1));
        memset(grid[TROWS - 1], ' ', TCOLS);
        grid[TROWS - 1][TCOLS] = 0;
        cy = TROWS - 1;
        mark_all();
    }
}

void term_putc(char c)
{
    uint32_t irq = irq_save();
    dirty |= 1u << cy;
    switch (c) {
    case '\r': cx = 0; break;
    case '\n': newline(); break;
    case '\b': if (cx > 0) cx--; break;
    case '\t': do { grid[cy][cx] = ' '; if (++cx >= TCOLS) { newline(); break; } } while (cx & 7); break;
    default:
        if (c >= 32 && c < 127) {
            grid[cy][cx] = c;
            if (++cx >= TCOLS) newline();
        }
        break;
    }
    dirty |= 1u << cy;
    irq_restore(irq);
}

void term_flush(void)
{
    uint32_t irq = irq_save();
    int ccol = cx, crow = cy;
    if (shown_row >= 0 && (shown_row != crow || shown_col != ccol)) dirty |= 1u << shown_row;
    dirty |= 1u << crow;
    uint32_t todo = dirty;
    dirty = 0;
    shown_row = crow; shown_col = ccol;
    irq_restore(irq);

    for (int r = 0; r < TROWS; r++) {
        if (!(todo & (1u << r))) continue;
        char copy[TCOLS + 1];
        irq = irq_save();
        memcpy(copy, grid[r], sizeof copy);
        irq_restore(irq);
        BIOS->lcd_draw_row(r, copy, r == crow ? ccol : -1, FG, BG);
    }
}
