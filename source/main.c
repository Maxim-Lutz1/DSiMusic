#include <nds.h>
#include <stdio.h>

static PrintConsole topConsole;
static PrintConsole bottomConsole;

static const char *albums[] = {
    "Sempiternal",
    "Suicide Season",
    "That's the Spirit"
};

#define ALBUM_COUNT 3

static int selectedAlbum = 0;

static void drawTop(void)
{
    consoleSelect(&topConsole);
    consoleClear();

    printf("\n");
    printf("        DSiMusic v0.1\n");
    printf("      ----------------\n\n");
    printf("  Your music. Your library.\n\n");
    printf("  Selected album:\n\n");
    printf("  %s\n\n", albums[selectedAlbum]);
    printf("  Status: UI prototype\n");
    printf("  Audio: not implemented\n");
}

static void drawBottom(void)
{
    consoleSelect(&bottomConsole);
    consoleClear();

    printf("\n");
    printf("  ALBUM LIBRARY\n");
    printf("  --------------------------\n\n");

    for (int i = 0; i < ALBUM_COUNT; i++)
    {
        printf("  %c %d. %s\n\n",
                i == selectedAlbum ? '>' : ' ',
                i + 1,
                albums[i]);
    }

    printf("\n  UP/DOWN: Select album\n");
    printf("  Touch: Select album\n");
}

int main(void)
{
    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);

    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);

    consoleInit(&topConsole, 0, BgType_Text4bpp,
                BgSize_T_256x256, 31, 0, true, true);

    consoleInit(&bottomConsole, 0, BgType_Text4bpp,
                BgSize_T_256x256, 31, 0, false, true);

    drawTop();
    drawBottom();

    while (1)
    {
        swiWaitForVBlank();
        scanKeys();

        int keys = keysDown();
        int previous = selectedAlbum;

        if (keys & KEY_DOWN)
            selectedAlbum = (selectedAlbum + 1) % ALBUM_COUNT;

        if (keys & KEY_UP)
            selectedAlbum =
                (selectedAlbum + ALBUM_COUNT - 1) % ALBUM_COUNT;

        if (keys & KEY_TOUCH)
        {
            touchPosition touch;
            touchRead(&touch);

            int y = touch.py;

            for (int i = 0; i < ALBUM_COUNT; i++)
            {
                int rowY = 48 + i * 32;

                if (y >= rowY && y < rowY + 24)
                    selectedAlbum = i;
            }
        }

        if (selectedAlbum != previous)
        {
            drawTop();
            drawBottom();
        }
    }

    return 0;
}
