#include <nds.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../../include/wav_preview.h"

#define RATE 44100
#define FRAMES 44100

/* One second of stereo PCM, split into two mono buffers. */
static int16_t left[FRAMES] __attribute__((aligned(32)));
static int16_t right[FRAMES] __attribute__((aligned(32)));

static int leftChannel = -1;
static int rightChannel = -1;

static uint16_t read16(FILE *f)
{
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static uint32_t read32(FILE *f)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

void wavPreviewStop(void)
{
    if (leftChannel >= 0) soundKill(leftChannel);
    if (rightChannel >= 0) soundKill(rightChannel);
    leftChannel = -1;
    rightChannel = -1;
}

const char *wavPreviewPlay(const char *path)
{
    wavPreviewStop();

    FILE *f = fopen(path, "rb");
    if (!f) return "WAV OPEN FAILED";

    char magic[4];

    if (fread(magic, 1, 4, f) != 4 ||
        memcmp(magic, "RIFF", 4)) {
        fclose(f);
        return "NOT A RIFF WAV";
    }

    read32(f);

    if (fread(magic, 1, 4, f) != 4 ||
        memcmp(magic, "WAVE", 4)) {
        fclose(f);
        return "NOT A WAV FILE";
    }

    int validFormat = 0;
    uint32_t dataSize = 0;
    int foundData = 0;

    /* RIFF chunks can appear in different orders. */
    while (fread(magic, 1, 4, f) == 4) {
        uint32_t size = read32(f);
        long start = ftell(f);

        if (!memcmp(magic, "fmt ", 4) && size >= 16) {
            uint16_t format = read16(f);
            uint16_t channels = read16(f);
            uint32_t rate = read32(f);
            read32(f);
            uint16_t alignment = read16(f);
            uint16_t bits = read16(f);

            validFormat = format == 1 &&
                          channels == 2 &&
                          rate == RATE &&
                          alignment == 4 &&
                          bits == 16;
        }

        if (!memcmp(magic, "data", 4)) {
            dataSize = size;
            foundData = 1;
            break;
        }

        if (fseek(f, start + size + (size & 1), SEEK_SET))
            break;
    }

    if (!validFormat || !foundData) {
        fclose(f);
        return "NEED PCM16 44K STEREO";
    }

    uint32_t frames = dataSize / 4;
    if (frames > FRAMES) frames = FRAMES;
    frames &= ~1u;

    if (frames < 4) {
        fclose(f);
        return "WAV DATA TOO SHORT";
    }

    uint32_t loaded = 0;

    for (; loaded < frames; loaded++) {
        uint16_t l = read16(f);
        uint16_t r = read16(f);

        if (ferror(f) || feof(f))
            break;

        left[loaded] = (int16_t)l;
        right[loaded] = (int16_t)r;
    }

    fclose(f);

    loaded &= ~1u;
    if (loaded < 4) return "WAV READ FAILED";

    /* Make buffers visible to the ARM7 audio processor. */
    DC_FlushRange(left, loaded * sizeof(int16_t));
    DC_FlushRange(right, loaded * sizeof(int16_t));

    soundEnable();
    soundSetMasterVolume(100);

    leftChannel = soundPlaySample(
        left, SoundFormat_16Bit,
        loaded * sizeof(int16_t),
        RATE, 100, 0, false, 0
    );

    rightChannel = soundPlaySample(
        right, SoundFormat_16Bit,
        loaded * sizeof(int16_t),
        RATE, 100, 127, false, 0
    );

    if (leftChannel < 0 || rightChannel < 0) {
        wavPreviewStop();
        return "AUDIO CHANNEL ERROR";
    }

    return "PLAYED WAV PREVIEW";
}
