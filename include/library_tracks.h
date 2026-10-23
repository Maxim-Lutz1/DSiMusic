#ifndef DSIMUSIC_LIBRARY_TRACKS_H
#define DSIMUSIC_LIBRARY_TRACKS_H

#define MAX_TRACKS 48
#define TRACK_NAME_SIZE 96

typedef struct {
    char name[TRACK_NAME_SIZE];
} LibraryTrack;

int libraryTracksScan(const char *albumPath,
                      LibraryTrack *tracks,
                      int capacity);

#endif
