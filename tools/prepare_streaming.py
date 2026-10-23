#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import wave
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("album", type=Path)
parser.add_argument("--backup", type=Path, required=True)
args = parser.parse_args()

album = args.album.resolve()
backup = args.backup.expanduser().resolve()
backup.mkdir(parents=True, exist_ok=True)

tracks = sorted(
    p for p in album.glob("*.wav")
    if p.name[:2].isdigit()
    and not p.name.startswith("00 - ")
    and not p.name.endswith(".stream.wav")
)

if not tracks:
    raise SystemExit("Keine Album-Tracks gefunden.")

for source in tracks:
    original = backup / source.name

    if not original.exists():
        shutil.copy2(source, original)

    with wave.open(str(original), "rb") as reader:
        if (
            reader.getframerate() == 32768
            and reader.getnchannels() == 2
            and reader.getsampwidth() == 2
        ):
            print("Already compatible:", source.name)
            continue

    target = source.with_name(source.name + ".tmp")
    command = [
        "ffmpeg", "-nostdin", "-v", "error",
        "-i", str(original), "-map", "0:a:0",
        "-f", "s16le", "-acodec", "pcm_s16le",
        "-ar", "32768", "-ac", "2", "pipe:1"
    ]

    print("Converting:", source.name, flush=True)
    try:
        with subprocess.Popen(command, stdout=subprocess.PIPE) as process:
            with wave.open(str(target), "wb") as output:
                output.setnchannels(2)
                output.setsampwidth(2)
                output.setframerate(32768)

                while True:
                    chunk = process.stdout.read(65536)
                    if not chunk:
                        break
                    output.writeframesraw(chunk)

            if process.wait() != 0:
                raise RuntimeError("FFmpeg failed")

        os.replace(target, source)
    finally:
        if target.exists():
            target.unlink()

(album / "tracks.txt").write_text(
    "\n".join(p.name for p in tracks) + "\n",
    encoding="utf-8"
)

print(f"Done: {len(tracks)} tracks")
print("Original WAV backup:", backup)
