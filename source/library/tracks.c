#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "../../include/library_tracks.h"

static int supported(const char *name)
{
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;

    char lower[8] = {0};
    size_t length = strlen(ext);

    if (length >= sizeof(lower))
        return 0;

    for (size_t i = 0; i < length; ++i)
        lower[i] = (char)tolower((unsigned char)ext[i]);

    return strcmp(lower, ".wav") == 0 ||
           strcmp(lower, ".flac") == 0;
}

int libraryTracksScan(const char *albumPath,
                      LibraryTrack *tracks,
                      int capacity)
{
    if (!albumPath || !tracks || capacity <= 0)
        return -1;

    DIR *dir = opendir(albumPath);
    if (!dir) return -1;

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) && count < capacity) {
        if (entry->d_name[0] == '.' ||
            !supported(entry->d_name))
            continue;

        char fullPath[512];

        int length = snprintf(fullPath, sizeof(fullPath),
                              "%s/%s", albumPath,
                              entry->d_name);

        if (length < 0 || length >= (int)sizeof(fullPath))
            continue;

        struct stat st;
        if (stat(fullPath, &st) != 0 || !S_ISREG(st.st_mode))
            continue;

        snprintf(tracks[count].name,
                 sizeof(tracks[count].name),
                 "%s", entry->d_name);

        count++;
    }

    closedir(dir);

    /* Alphabetical sorting, including numbered filenames. */
    for (int i = 1; i < count; ++i) {
        LibraryTrack current = tracks[i];
        int j = i - 1;

        while (j >= 0 &&
               strcmp(tracks[j].name, current.name) > 0) {
            tracks[j + 1] = tracks[j];
            j--;
        }

        tracks[j + 1] = current;
    }

    return count;
}
