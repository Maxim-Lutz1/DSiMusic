#ifndef DSIMUSIC_WAV_PREVIEW_H
#define DSIMUSIC_WAV_PREVIEW_H

/* Returns an error message or a successful playback status. */
const char *wavPreviewPlay(const char *path);
void wavPreviewStop(void);

#endif
