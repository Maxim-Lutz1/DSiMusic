#ifndef DSIMUSIC_LIBRARY_SCANNER_H
#define DSIMUSIC_LIBRARY_SCANNER_H

#define MAX_ALBUMS 48
#define LIB_NAME_SIZE 48

typedef struct {
    char title[LIB_NAME_SIZE];
    char artist[LIB_NAME_SIZE];
    char path[384];
} LibraryAlbum;

/* Returns number of albums, or -1 if the library cannot be opened. */
int libraryScan(LibraryAlbum *albums, int maxAlbums);

#endif
