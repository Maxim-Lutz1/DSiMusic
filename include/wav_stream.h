#ifndef DSIMUSIC_WAV_STREAM_H
#define DSIMUSIC_WAV_STREAM_H

#include <stdbool.h>

bool wavStreamInit(void);
const char *wavStreamPlay(const char *path);
void wavStreamUpdate(void);
void wavStreamStop(void);
bool wavStreamIsPlaying(void);
unsigned wavStreamSeconds(void);
unsigned wavStreamDuration(void);

#endif
