#include <nds.h>
#include <maxmod9.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "../../include/wav_stream.h"

/*
 * Initial streaming backend:
 * PCM signed 16-bit, stereo, 32768 Hz.
 * A future backend will target native 44100 Hz.
 */

#define STREAM_RATE 32768
#define STREAM_BUFFER 8192

static FILE *audioFile = NULL;
static uint32_t bytesRemaining = 0;
static uint32_t totalFrames = 0;
static bool streamInitialized = false;
static bool playing = false;

static uint16_t read16(FILE *f)
{
    uint8_t b[2];

    if (fread(b, 1, 2, f) != 2)
        return 0;

    return (uint16_t)b[0] |
           ((uint16_t)b[1] << 8);
}

static uint32_t read32(FILE *f)
{
    uint8_t b[4];

    if (fread(b, 1, 4, f) != 4)
        return 0;

    return (uint32_t)b[0] |
           ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
}

/* Maxmod calls this to refill its stereo audio buffer. */
static mm_word streamCallback(
    mm_word length,
    mm_addr dest,
    mm_stream_formats format)
{
    (void)format;

    size_t wanted = (size_t)length * 4;
    size_t available = bytesRemaining;

    if (available > wanted)
        available = wanted;

    size_t received = 0;

    if (audioFile && available > 0)
        received = fread(dest, 1, available, audioFile);

    bytesRemaining -= (uint32_t)received;

    /* Fill remaining samples with silence at EOF. */
    if (received < wanted)
        memset((uint8_t *)dest + received,
               0, wanted - received);

    return length;
}

bool wavStreamInit(void)
{
    if (streamInitialized)
        return true;

    streamInitialized = mmInitNoSoundbank();
    return streamInitialized;
}

void wavStreamStop(void)
{
    if (playing) {
        mmStreamClose();
        playing = false;
    }

    if (audioFile) {
        fclose(audioFile);
        audioFile = NULL;
    }

    bytesRemaining = 0;
    totalFrames = 0;
}

const char *wavStreamPlay(const char *path)
{
    wavStreamStop();

    if (!streamInitialized)
        return "MAXMOD NOT INITIALIZED";

    fprintf(stderr, "[AUDIO] Opening: %s\n", path);

    errno = 0;
    FILE *f = fopen(path, "rb");

    if (!f) {
        fprintf(stderr,
                "[AUDIO] fopen FAILED: errno=%d (%s)\n",
                errno, strerror(errno));
        return "FILE OPEN FAILED";
    }

    fprintf(stderr, "[AUDIO] fopen SUCCESS\n");

    char id[4];

    if (fread(id, 1, 4, f) != 4 ||
        memcmp(id, "RIFF", 4) != 0) {
        fclose(f);
        return "NOT RIFF WAV";
    }

    read32(f);

    if (fread(id, 1, 4, f) != 4 ||
        memcmp(id, "WAVE", 4) != 0) {
        fclose(f);
        return "NOT WAVE";
    }

    bool formatValid = false;
    bool dataFound = false;
    uint32_t dataBytes = 0;

    while (fread(id, 1, 4, f) == 4) {
        uint32_t size = read32(f);
        long start = ftell(f);

        if (start < 0)
            break;

        if (!memcmp(id, "fmt ", 4) && size >= 16) {
            uint16_t codec = read16(f);
            uint16_t channels = read16(f);
            uint32_t rate = read32(f);
            read32(f);
            uint16_t blockAlign = read16(f);
            uint16_t bits = read16(f);

            formatValid =
                codec == 1 &&
                channels == 2 &&
                rate == STREAM_RATE &&
                blockAlign == 4 &&
                bits == 16;
        }

        if (!memcmp(id, "data", 4)) {
            dataBytes = size;
            dataFound = true;
            break;
        }

        if (fseek(f, start + size + (size & 1u),
                  SEEK_SET) != 0)
            break;
    }

    if (!formatValid || !dataFound ||
        dataBytes < 16) {
        fclose(f);
        return "NEED PCM16 32768 STEREO";
    }

    audioFile = f;
    bytesRemaining = dataBytes & ~3u;
    totalFrames = bytesRemaining / 4;

    mm_stream stream = {0};
    stream.sampling_rate = STREAM_RATE;
    stream.buffer_length = STREAM_BUFFER;
    stream.callback = streamCallback;
    stream.format = MM_STREAM_16BIT_STEREO;
    stream.timer = MM_TIMER0;
    stream.manual = true;

    mmStreamOpen(&stream);
    playing = true;

    /* Pre-fill the audio buffer. */
    mmStreamUpdate();

    return "STREAMING WAV";
}

void wavStreamUpdate(void)
{
    if (!playing)
        return;

    mmStreamUpdate();

    if (mmStreamGetPosition() >= totalFrames)
        wavStreamStop();
}

bool wavStreamIsPlaying(void)
{
    return playing;
}

unsigned wavStreamSeconds(void)
{
    if (!playing)
        return 0;

    return mmStreamGetPosition() / STREAM_RATE;
}

unsigned wavStreamDuration(void)
{
    return totalFrames / STREAM_RATE;
}
