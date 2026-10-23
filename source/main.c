#include <nds.h>
#include "font_generated.h"
#include "../include/library_scanner.h"
#include "../include/library_tracks.h"
#include "../include/wav_preview.h"
#include <fat.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "covers_generated.h"

#define W 256
#define H 192

#define COL(r,g,b) (RGB15(r,g,b) | BIT(15))

static u16 *top;
static u16 *bottom;
static int selected = 0;
static const char *sdStatus = "SD NOT TESTED";


static LibraryAlbum albums[MAX_ALBUMS];
static int albumCount = 0;

#define COVER_SIZE 64
#define COVER_PIXELS (COVER_SIZE * COVER_SIZE)

static u16 coverPixels[MAX_ALBUMS][COVER_PIXELS];
static int coverLoaded[MAX_ALBUMS];

static void loadAlbumCovers(void)
{
    for (int i = 0; i < albumCount; i++) {
        coverLoaded[i] = 0;

        char path[512];
        int n = snprintf(path, sizeof(path),
                         "%s/cover.bin", albums[i].path);

        if (n < 0 || n >= (int)sizeof(path))
            continue;

        FILE *file = fopen(path, "rb");
        if (!file)
            continue;

        size_t count = fread(coverPixels[i], sizeof(u16),
                             COVER_PIXELS, file);
        fclose(file);

        if (count == COVER_PIXELS)
            coverLoaded[i] = 1;
    }
}

static LibraryTrack tracks[MAX_TRACKS];
static int trackCount = 0;
static int trackSelected = 0;
static int trackView = 0;
static const char *audioStatus = "A PLAY WAV PREVIEW";


static const char *libraryStatus = "SCANNING";
static const u16 fallbackColors[MAX_ALBUMS] = {
    COL(20,18,22), COL(21,8,8), COL(13,13,22),
    COL(24,13,20), COL(15,21,18), COL(16,10,17)
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
                 const char *str, int scale, u16 color)
{
    int originX = x;

    if (scale < 1)
        scale = 1;

    while (*str) {
        unsigned char c = (unsigned char)*str++;

        if (c == '\n') {
            x = originX;
            y += FONT_HEIGHT * scale + 2;
            continue;
        }

        if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT)
            c = '?';

        int index = c - FONT_FIRST;
        int advance = fontWidth[index] * scale;

        /* Clip long lines instead of drawing into invalid memory. */
        if (x >= W || y >= H)
            break;

        for (int row = 0; row < FONT_HEIGHT; ++row) {
            uint16_t bits = fontBitmap[index][row];

            for (int col = 0; col < 12; ++col) {
                if (bits & (1u << col)) {
                    rect(screen,
                         x + col * scale,
                         y + row * scale,
                         scale, scale, color);
                }
            }
        }

        x += advance;
    }
}

static void cover(u16 *screen, int x, int y, int size, int id)
{
    if (id < 0 || id >= albumCount)
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
                (coverLoaded[id]
                    ? coverPixels[id][sy * 64 + sx]
                    : (((sx - 32) * (sx - 32) +
                         (sy - 32) * (sy - 32) < 350)
                         ? COL(21,18,27)
                         : COL(9,8,13)));
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


static void textWrapped(u16 *screen, int x, int y,
                        const char *str, int maxWidth, u16 color)
{
    char line[64];
    int lineLen = 0;
    int lineWidth = 0;
    int lines = 0;

    while (*str && lines < 2) {
        while (*str == ' ')
            str++;

        char word[64];
        int wordLen = 0;
        int wordWidth = 0;

        while (*str && *str != ' ' && wordLen < 62) {
            unsigned char c = (unsigned char)*str++;
            word[wordLen++] = c;

            if (c >= FONT_FIRST &&
                c < FONT_FIRST + FONT_COUNT)
                wordWidth += fontWidth[c - FONT_FIRST];
            else
                wordWidth += fontWidth['?' - FONT_FIRST];
        }
        word[wordLen] = '\0';

        int spaceWidth = fontWidth[' ' - FONT_FIRST];

        if (lineLen > 0 &&
            lineWidth + spaceWidth + wordWidth > maxWidth) {
            line[lineLen] = '\0';
            text(screen, x, y + lines * 17, line, 1, color);
            lines++;
            lineLen = 0;
            lineWidth = 0;
        }

        if (lines >= 2)
            break;

        if (lineLen > 0) {
            line[lineLen++] = ' ';
            lineWidth += spaceWidth;
        }

        for (int i = 0; i < wordLen && lineLen < 62; i++) {
            unsigned char c = (unsigned char)word[i];
            int advance = (c >= FONT_FIRST &&
                           c < FONT_FIRST + FONT_COUNT)
                          ? fontWidth[c - FONT_FIRST]
                          : fontWidth['?' - FONT_FIRST];

            if (lineWidth + advance > maxWidth)
                break;

            line[lineLen++] = (char)c;
            lineWidth += advance;
        }
    }

    if (lineLen > 0 && lines < 2) {
        line[lineLen] = '\0';
        text(screen, x, y + lines * 17, line, 1, color);
    }
}


static int textPixelWidth(const char *str)
{
    int width = 0;

    while (*str) {
        unsigned char c = (unsigned char)*str++;

        if (c < FONT_FIRST ||
            c >= FONT_FIRST + FONT_COUNT)
            c = '?';

        width += fontWidth[c - FONT_FIRST];
    }

    return width;
}

static void textFit(u16 *screen, int x, int y,
                    const char *str, int maxWidth, u16 color)
{
    char buffer[128];

    snprintf(buffer, sizeof(buffer), "%s", str);

    if (textPixelWidth(buffer) > maxWidth) {
        int len = strlen(buffer);

        while (len > 0) {
            buffer[--len] = '\0';

            if (textPixelWidth(buffer) +
                textPixelWidth("...") <= maxWidth)
                break;
        }

        if (maxWidth >= textPixelWidth("..."))
            strcat(buffer, "...");
    }

    text(screen, x, y, buffer, 1, color);
}

static void drawTop(void)
{
    rect(top, 0, 0, W, H, COL(3,3,6));
    rect(top, 0, 0, W, 29, COL(9,7,15));

    text(top, 13, 8, "DSIMUSIC", 1, COL(26,20,31));
    text(top, 195, 12, "V0-2", 1, COL(20,19,24));

    if (albumCount == 0) {
        text(top, 15, 60, libraryStatus, 1, COL(31,22,22));
        return;
    }

    cover(top, 14, 43, 102, selected);

    text(top, 128, 47, "SELECTED", 1, COL(20,17,27));
    textFit(top, 128, 66, albums[selected].title,
            116, COL(31,31,31));

    text(top, 128, 94, "ARTIST", 1, COL(20,17,27));
    textWrapped(top, 128, 108, albums[selected].artist,
                116, COL(31,31,31));

    rect(top, 12, 160, 232, 1, COL(14,12,21));
    text(top, 15, 173, audioStatus, 1, COL(19,27,21));
}




static void formatTrackName(char *dest, int capacity,
                            const char *filename)
{
    int i = 0;

    while (filename[i] && i < capacity - 1) {
        dest[i] = filename[i];
        i++;
    }

    dest[i] = '\0';

    char *dot = strrchr(dest, '.');
    if (dot && (!strcasecmp(dot, ".wav") ||
                !strcasecmp(dot, ".flac")))
        *dot = '\0';
}

static void drawBottom(void)
{
    rect(bottom, 0, 0, W, H, COL(3,3,6));
    rect(bottom, 0, 0, W, 31, COL(9,7,15));

    if (trackView) {
        text(bottom, 12, 8, "TRACK LIST", 1, COL(28,25,31));

        if (trackCount < 0) {
            text(bottom, 12, 48, "TRACK SCAN FAILED",
                 1, COL(31,18,18));
        } else if (trackCount == 0) {
            text(bottom, 12, 48, "NO WAV OR FLAC FILES",
                 1, COL(31,26,26));
        } else {
            int first = 0;

            if (trackSelected >= 7)
                first = trackSelected - 6;

            for (int row = 0;
                 row < 7 && first + row < trackCount;
                 ++row) {

                int index = first + row;
                int y = 38 + row * 18;

                if (index == trackSelected)
                    rect(bottom, 6, y - 2, 244, 17,
                         COL(15,11,22));

                text(bottom, 10, y,
                     index == trackSelected ? ">" : " ",
                     1, COL(29,22,31));

                char displayName[49];
                formatTrackName(displayName, sizeof(displayName),
                                tracks[index].name);

                textFit(bottom, 22, y,
                        displayName,
                        222, COL(28,28,30));
            }
        }

        rect(bottom, 0, 171, W, 21, COL(9,7,15));
        text(bottom, 12, 174, "B BACK  UP DOWN SELECT",
             1, COL(24,22,29));
        return;
    }

    text(bottom, 12, 10, "YOUR LIBRARY",
         2, COL(28,25,31));

    for (int i = 0; i < albumCount; ++i) {
        int col = i % 3;
        int row = i / 3;
        int x = 16 + col * 80;
        int y = 39 + row * 66;

        if (i == selected)
            rect(bottom, x - 3, y - 3, 70, 70,
                 COL(25,19,31));

        cover(bottom, x, y, 64, i);
    }

    rect(bottom, 0, 171, W, 21, COL(9,7,15));
    text(bottom, 12, 174, "A OPEN  DPAD OR TOUCH",
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

    albumCount = libraryScan(albums, MAX_ALBUMS);

    if (albumCount < 0) {
        albumCount = 0;
        libraryStatus = "SD SCAN FAILED";
    } else if (albumCount == 0) {
        libraryStatus = "NO ALBUMS FOUND";
    }

    loadAlbumCovers();
    drawAll();


    while (1) {
        swiWaitForVBlank();
        scanKeys();

        int keys = keysDown();
        int old = selected;
        int oldTrack = trackSelected;
        int oldView = trackView;
        int audioChanged = 0;

        if (trackView) {
            if ((keys & KEY_A) && trackCount > 0) {
                char path[512];
                const char *name = tracks[trackSelected].name;

                if (snprintf(path, sizeof(path), "%s/%s",
                             albums[selected].path, name)
                    < (int)sizeof(path)) {
                    audioStatus = wavPreviewPlay(path);
                } else {
                    audioStatus = "TRACK PATH TOO LONG";
                }

                audioChanged = 1;
            }

            if (keys & KEY_B) {
                wavPreviewStop();
                audioStatus = "A PLAY WAV PREVIEW";
                audioChanged = 1;
                trackView = 0;
            } else if (trackCount > 0) {
                if (keys & KEY_DOWN)
                    trackSelected = (trackSelected + 1) % trackCount;
                if (keys & KEY_UP)
                    trackSelected =
                        (trackSelected + trackCount - 1) % trackCount;
            }
        } else if (albumCount > 0) {
            if (keys & KEY_RIGHT)
                selected = (selected + 1) % albumCount;
            if (keys & KEY_LEFT)
                selected = (selected + albumCount - 1) % albumCount;
            if (keys & KEY_DOWN)
                selected = (selected + 3) % albumCount;
            if (keys & KEY_UP)
                selected =
                    (selected + albumCount - 3 % albumCount)
                    % albumCount;

            if (keys & KEY_TOUCH) {
                touchPosition touch;
                touchRead(&touch);

                for (int i = 0; i < albumCount; i++) {
                    int x = 16 + (i % 3) * 80;
                    int y = 39 + (i / 3) * 66;

                    if (touch.px >= x && touch.px < x + 64 &&
                        touch.py >= y && touch.py < y + 64)
                        selected = i;
                }
            }

            if (keys & KEY_A) {
                trackCount = libraryTracksScan(
                    albums[selected].path,
                    tracks, MAX_TRACKS
                );
                trackSelected = 0;
                trackView = 1;
            }
        }

        if (selected != old || trackSelected != oldTrack ||
            trackView != oldView || audioChanged) {
            drawAll();
        }
    }

    return 0;
}
