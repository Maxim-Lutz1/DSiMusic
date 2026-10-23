#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "covers_generated.h"

#define W 256
#define H 192
#define COUNT 6

#define COL(r,g,b) (RGB15(r,g,b) | BIT(15))

static u16 *top;
static u16 *bottom;
static int selected = 0;
static const char *sdStatus = "SD NOT TESTED";

typedef struct {
    const char *title;
    const char *artist;
    u16 color;
} Album;

static const Album albums[COUNT] = {
    {"SEMPITERNAL",    "BRING ME THE HORIZON", COL(20,18,22)},
    {"SUICIDE SEASON", "BRING ME THE HORIZON", COL(21,8,8)},
    {"THATS THE SPIRIT","BRING ME THE HORIZON",COL(13,13,22)},
    {"AMO",            "BRING ME THE HORIZON", COL(24,13,20)},
    {"SURVIVAL HORROR","BRING ME THE HORIZON", COL(15,21,18)},
    {"COUNT YOUR",     "BRING ME THE HORIZON", COL(16,10,17)}
};

/* 3x5 pixel font: A-Z, 0-9 */
static const u8 font[36][5] = {
    {2,5,7,5,5}, {6,5,6,5,6}, {3,4,4,4,3},
    {6,5,5,5,6}, {7,4,6,4,7}, {7,4,6,4,4},
    {3,4,5,5,3}, {5,5,7,5,5}, {7,2,2,2,7},
    {1,1,1,5,2}, {5,5,6,5,5}, {4,4,4,4,7},
    {5,7,7,5,5}, {5,7,7,7,5}, {2,5,5,5,2},
    {6,5,6,4,4}, {2,5,5,7,3}, {6,5,6,5,5},
    {3,4,2,1,6}, {7,2,2,2,2}, {5,5,5,5,7},
    {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5},
    {5,5,2,2,2}, {7,1,2,4,7},
    {7,5,5,5,7}, {2,6,2,2,7}, {6,1,7,4,7},
    {6,1,3,1,6}, {5,5,7,1,1}, {7,4,6,1,6},
    {3,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7},
    {7,5,7,1,6}
};

static void rect(u16 *screen, int x, int y, int w, int h, u16 color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0) return;

    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            screen[yy * W + xx] = color;
}

static void text(u16 *screen, int x, int y,
                 const char *s, int scale, u16 color)
{
    int startX = x;

    while (*s) {
        char ch = *s++;

        if (ch == '\n') {
            x = startX;
            y += 6 * scale;
            continue;
        }

        int id = -1;
        if (ch >= 'A' && ch <= 'Z') id = ch - 'A';
        if (ch >= '0' && ch <= '9') id = 26 + ch - '0';

        if (id >= 0) {
            for (int row = 0; row < 5; row++)
                for (int col = 0; col < 3; col++)
                    if (font[id][row] & (1 << (2 - col)))
                        rect(screen, x + col * scale,
                             y + row * scale, scale, scale, color);
        }

        if (ch == '-')
            rect(screen, x, y + 2 * scale, 3 * scale, scale, color);

        x += 4 * scale;
    }
}


static void cover(u16 *screen, int x, int y, int size, int id)
{
    if (id < 0 || id >= COUNT)
        return;

    for (int dy = 0; dy < size; dy++) {
        int sy = dy * 64 / size;
        int py = y + dy;

        if (py < 0 || py >= H)
            continue;

        for (int dx = 0; dx < size; dx++) {
            int sx = dx * 64 / size;
            int px = x + dx;

            if (px < 0 || px >= W)
                continue;

            screen[py * W + px] =
                albumCovers[id][sy * 64 + sx];
        }
    }
}

static void testSD(void)
{
    if (!fatInitDefault()) {
        sdStatus = "SD INIT FAILED";
        return;
    }

    FILE *f = fopen("sd:/Music/test.txt", "rb");

    if (!f) {
        /* Alternative for DS-mode flashcart setups */
        f = fopen("fat:/Music/test.txt", "rb");
    }

    if (!f) {
        sdStatus = "TEST FILE NOT FOUND";
        return;
    }

    char buffer[64] = {0};
    size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);

    if (n > 0 && strstr(buffer, "DSiMusic SD test OK"))
        sdStatus = "SD READ SUCCESS";
    else
        sdStatus = "SD READ ERROR";
}

static void drawTop(void)
{
    rect(top, 0, 0, W, H, COL(3,3,6));
    rect(top, 0, 0, W, 29, COL(9,7,15));

    text(top, 13, 9, "DSIMUSIC", 2, COL(26,20,31));
    text(top, 195, 12, "V0-2", 1, COL(20,19,24));

    cover(top, 14, 43, 102, selected);

    text(top, 128, 47, "SELECTED", 1, COL(20,17,27));
    text(top, 128, 66, albums[selected].title,
         1, COL(31,31,31));

    text(top, 128, 94, "ARTIST", 1, COL(20,17,27));
    text(top, 128, 108, "BRING ME", 1, COL(31,31,31));
    text(top, 128, 118, "THE HORIZON", 1, COL(31,31,31));

    rect(top, 12, 160, 232, 1, COL(14,12,21));
    text(top, 15, 173, sdStatus, 1, COL(19,27,21));
}

static void drawBottom(void)
{
    rect(bottom, 0, 0, W, H, COL(3,3,6));
    rect(bottom, 0, 0, W, 31, COL(9,7,15));

    text(bottom, 12, 10, "YOUR LIBRARY", 2, COL(28,25,31));

    for (int i = 0; i < COUNT; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = 16 + col * 80;
        int y = 39 + row * 69;

        if (i == selected)
            rect(bottom, x - 3, y - 3, 70, 70,
                 COL(25,19,31));

        cover(bottom, x, y, 64, i);
    }

    rect(bottom, 0, 181, W, 11, COL(9,7,15));
    text(bottom, 12, 184, "DPAD OR TOUCH TO SELECT",
         1, COL(24,22,29));
}

static void drawAll(void)
{
    drawTop();
    drawBottom();
}

int main(void)
{
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    int topBg = bgInit(2, BgType_Bmp16,
                       BgSize_B16_256x256, 0, 0);
    int bottomBg = bgInitSub(2, BgType_Bmp16,
                             BgSize_B16_256x256, 0, 0);

    top = bgGetGfxPtr(topBg);
    bottom = bgGetGfxPtr(bottomBg);

    testSD();
    drawAll();

    while (1) {
        swiWaitForVBlank();
        scanKeys();

        int keys = keysDown();
        int old = selected;

        if (keys & KEY_RIGHT)
            selected = (selected + 1) % COUNT;

        if (keys & KEY_LEFT)
            selected = (selected + COUNT - 1) % COUNT;

        if (keys & KEY_DOWN)
            selected = (selected + 3) % COUNT;

        if (keys & KEY_UP)
            selected = (selected + 3) % COUNT;

        if (keys & KEY_TOUCH) {
            touchPosition touch;
            touchRead(&touch);

            for (int i = 0; i < COUNT; i++) {
                int x = 16 + (i % 3) * 80;
                int y = 39 + (i / 3) * 69;

                if (touch.px >= x && touch.px < x + 64 &&
                    touch.py >= y && touch.py < y + 64)
                    selected = i;
            }
        }

        if (selected != old)
            drawAll();
    }

    return 0;
}
