#!/usr/bin/env python3

from pathlib import Path
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("music_directory", type=Path)
args = parser.parse_args()

root = args.music_directory.expanduser().resolve()

if not root.is_dir():
    parser.error(f"Music directory not found: {root}")

extensions = {".wav", ".flac"}

for album in sorted(root.glob("*/*")):
    if not album.is_dir():
        continue

    tracks = sorted(
        (
            file.name
            for file in album.iterdir()
            if file.is_file()
            and file.suffix.lower() in extensions
        ),
        key=str.casefold,
    )

    if not tracks:
        continue

    manifest = album / "tracks.txt"
    manifest.write_text(
        "\n".join(tracks) + "\n",
        encoding="utf-8",
    )

    print(f"[OK] {album.name}: {len(tracks)} tracks")

print("Tracklists generated.")
