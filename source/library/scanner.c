#include <nds.h>
#include <fat.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../../include/library_scanner.h"

static int isDirectory(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static void copyUpper(char *dest, const char *src, size_t capacity)
{
    size_t i = 0;

    while (src[i] && i + 1 < capacity) {
        dest[i] = (char)toupper((unsigned char)src[i]);
        i++;
    }

    dest[i] = '\0';
}

int libraryScan(LibraryAlbum *albums, int maxAlbums)
{
    if (!albums || maxAlbums <= 0)
        return -1;

    if (!fatInitDefault())
        return -1;

    const char *roots[] = {
        "sd:/Music",
        "fat:/Music"
    };

    for (int rootIndex = 0; rootIndex < 2; rootIndex++) {
        const char *root = roots[rootIndex];
        DIR *artists = opendir(root);

        if (!artists)
            continue;

        int count = 0;
        struct dirent *artistEntry;

        while ((artistEntry = readdir(artists)) != NULL &&
               count < maxAlbums) {

            const char *artist = artistEntry->d_name;

            if (artist[0] == '.')
                continue;

            char artistPath[256];

            if (snprintf(artistPath, sizeof(artistPath),
                         "%s/%s", root, artist)
                >= (int)sizeof(artistPath))
                continue;

            if (!isDirectory(artistPath))
                continue;

            DIR *albumDirs = opendir(artistPath);

            if (!albumDirs)
                continue;

            struct dirent *albumEntry;

            while ((albumEntry = readdir(albumDirs)) != NULL &&
                   count < maxAlbums) {

                const char *albumName = albumEntry->d_name;

                if (albumName[0] == '.')
                    continue;

                char albumPath[384];

                if (snprintf(albumPath, sizeof(albumPath),
                             "%s/%s", artistPath, albumName)
                    >= (int)sizeof(albumPath))
                    continue;

                if (!isDirectory(albumPath))
                    continue;

                copyUpper(albums[count].artist, artist,
                          sizeof(albums[count].artist));

                copyUpper(albums[count].title, albumName,
                          sizeof(albums[count].title));

                snprintf(albums[count].path,
                         sizeof(albums[count].path),
                         "%s", albumPath);

                count++;
            }

            closedir(albumDirs);
        }

        closedir(artists);
        return count;
    }

    return -1;
}
