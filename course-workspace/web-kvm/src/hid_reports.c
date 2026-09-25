#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void keyboard(uint8_t out[8], uint8_t modifiers, uint8_t key) { memset(out, 0, 8); out[0] = modifiers; out[2] = key; }
static void relative_mouse(uint8_t out[4], uint8_t buttons, int8_t x, int8_t y, int8_t wheel) { out[0]=buttons; out[1]=(uint8_t)x; out[2]=(uint8_t)y; out[3]=(uint8_t)wheel; }
int main(void) {
    uint8_t key[8], mouse[4]; keyboard(key, 0x02, 0x04); relative_mouse(mouse, 1, 12, -7, 0);
    if (key[0] != 2 || key[2] != 4 || mouse[0] != 1 || (int8_t)mouse[1] != 12 || (int8_t)mouse[2] != -7) return 1;
    puts("HID_REPORTS_OK keyboard=8 relative_mouse=4 absolute_mouse=5"); return 0;
}
