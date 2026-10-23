#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../../include/library_tracks.h"

static int supported(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;

    char lower[8] = {0};
    size_t length = strlen(ext);

    if (length >= sizeof(lower))
        return 0;

    for (size_t i = 0; i < length; i++)
        lower[i] = (char)tolower((unsigned char)ext[i]);

    return strcmp(lower, ".wav") == 0 ||
           strcmp(lower, ".flac") == 0;
}

static void sortTracks(LibraryTrack *tracks, int count)
{
    for (int i = 1; i < count; i++) {
        LibraryTrack current = tracks[i];
        int j = i - 1;

        while (j >= 0 &&
               strcmp(tracks[j].name, current.name) > 0) {
            tracks[j + 1] = tracks[j];
            j--;
        }

        tracks[j + 1] = current;
    }
}

int libraryTracksScan(const char *albumPath,
                      LibraryTrack *tracks,
                      int capacity)
{
    if (!albumPath || !tracks || capacity <= 0)
        return -1;

    char manifestPath[512];

    int n = snprintf(manifestPath, sizeof(manifestPath),
                     "%s/tracks.txt", albumPath);

    if (n < 0 || n >= (int)sizeof(manifestPath))
        return -1;

    FILE *manifest = fopen(manifestPath, "rb");

    if (manifest) {
        int count = 0;
        char line[512];

        while (count < capacity &&
               fgets(line, sizeof(line), manifest)) {

            char *newline = strpbrk(line, "\r\n");
            if (newline) *newline = '\0';

            size_t len = strlen(line);

            if (!len || len >= TRACK_NAME_SIZE)
                continue;

            if (strchr(line, '/') || strchr(line, '\\'))
                continue;

            if (!supported(line))
                continue;

            memcpy(tracks[count].name, line, len + 1);
            count++;
        }

        int failed = ferror(manifest);
        fclose(manifest);

        if (failed)
            return -1;

        sortTracks(tracks, count);
        return count;
    }

    /* Fallback for album folders without tracks.txt */
    DIR *dir = opendir(albumPath);
    if (!dir)
        return -1;

    int count = 0;
    struct dirent *entry;

    while (count < capacity &&
           (entry = readdir(dir)) != NULL) {

        const char *name = entry->d_name;
        size_t len = strlen(name);

        if (name[0] == '.' ||
            len >= TRACK_NAME_SIZE ||
            !supported(name))
            continue;

        memcpy(tracks[count].name, name, len + 1);
        count++;
    }

    closedir(dir);
    sortTracks(tracks, count);

    return count;
}
