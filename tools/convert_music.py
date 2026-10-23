#!/usr/bin/env python3
"""DSiMusic library converter: audio to PCM WAV + cover.bin + library.json.

Requires FFmpeg (ffmpeg, ffprobe) and ImageMagick 7 (magick).
The generated WAV is uncompressed 16-bit 32.768 kHz stereo PCM.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

EXTENSIONS = {'.flac', '.wav', '.mp3', '.m4a', '.mp4', '.aac', '.ogg', '.opus', '.aiff', '.aif', '.wma'}
IMAGE_NAMES = ('cover.jpg', 'cover.jpeg', 'cover.png', 'folder.jpg', 'folder.png', 'front.jpg', 'front.png')


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def clean(value, fallback):
    value = str(value or '').strip()
    value = re.sub(r'[\x00-\x1f<>:"/\\|?*]', '_', value)
    value = value.strip(' .')
    return value[:100] or fallback


def metadata(path):
    result = run(['ffprobe', '-v', 'error', '-show_entries',
                  'format_tags:stream=index,codec_type:stream_tags:stream_disposition=attached_pic',
                  '-of', 'json', str(path)], capture_output=True, text=True)
    data = json.loads(result.stdout)
    tags = {str(k).lower(): v for k, v in data.get('format', {}).get('tags', {}).items()}
    for stream in data.get('streams', []):
        if stream.get('codec_type') == 'audio':
            for k, v in stream.get('tags', {}).items():
                tags.setdefault(str(k).lower(), v)
    return tags, data.get('streams', [])


def image_rgb(path, streams):
    # Embedded cover first; external folder art second.
    if any(s.get('disposition', {}).get('attached_pic') == 1 for s in streams):
        try:
            picture = run(['ffmpeg', '-v', 'error', '-i', str(path),
                           '-map', '0:v:0', '-frames:v', '1',
                           '-f', 'image2pipe', '-vcodec', 'png', 'pipe:1'],
                          capture_output=True).stdout
            if picture:
                return run(['magick', 'png:-', '-auto-orient', '-resize', '64x64^',
                            '-gravity', 'center', '-extent', '64x64', '-alpha', 'remove',
                            '-depth', '8', 'rgb:-'], input=picture, capture_output=True).stdout
        except subprocess.CalledProcessError:
            pass
    for filename in IMAGE_NAMES:
        img = path.parent / filename
        if img.is_file():
            return run(['magick', str(img), '-auto-orient', '-resize', '64x64^',
                        '-gravity', 'center', '-extent', '64x64', '-alpha', 'remove',
                        '-depth', '8', 'rgb:-'], capture_output=True).stdout
    return None


def write_cover(rgb, output):
    if len(rgb) != 64 * 64 * 3:
        raise ValueError('Wrong image size')
    out = bytearray(64 * 64 * 2)
    for idx in range(64 * 64):
        r, g, b = rgb[idx * 3:idx * 3 + 3]
        v = 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
        out[idx * 2] = v & 255
        out[idx * 2 + 1] = v >> 8
    output.write_bytes(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='Folder containing original albums')
    parser.add_argument('output', type=Path, help='SD-card root, e.g. ./sdcard')
    parser.add_argument('--overwrite', action='store_true', help='Replace existing converted WAV files')
    args = parser.parse_args()
    for binary in ('ffmpeg', 'ffprobe', 'magick'):
        if not shutil.which(binary):
            parser.error(f'Missing required tool: {binary}')
    source = args.input.expanduser().resolve()
    target = args.output.expanduser().resolve()
    if not source.is_dir():
        parser.error(f'Input folder missing: {source}')
    if target == source or source in target.parents or target in source.parents:
        parser.error('Input and output must be separate directories (neither inside the other)')
    tracks = sorted((p for p in source.rglob('*') if p.is_file() and p.suffix.lower() in EXTENSIONS),
                    key=lambda p: str(p).casefold())
    if not tracks:
        parser.error('No supported audio files found')
    root = target / 'Music'
    root.mkdir(parents=True, exist_ok=True)
    albums = {}
    failed = 0
    for number, path in enumerate(tracks, 1):
        try:
            tags, streams = metadata(path)
            artist = clean(tags.get('album_artist') or tags.get('albumartist') or tags.get('artist') or
                           (path.parent.parent.name if path.parent.parent != source else ''), 'Unknown Artist')
            album = clean(tags.get('album') or path.parent.name, 'Unknown Album')
            title = clean(tags.get('title') or path.stem, 'Untitled')
            track_no = str(tags.get('track') or '').split('/')[0].strip()
            if not track_no.isdecimal():
                match = re.match(r'^(\d{1,3})\s*[-._ ]', path.stem)
                track_no = match.group(1) if match else ''
            prefix = f'{int(track_no):02d} - ' if track_no else ''
            folder = root / artist / album
            folder.mkdir(parents=True, exist_ok=True)
            output = folder / f'{prefix}{title}.wav'
            entry = albums.setdefault((artist, album), {'artist': artist, 'album': album,
                                                        'path': str(folder.relative_to(target)),
                                                        'tracks': [], 'cover': None})
            print(f'[{number}/{len(tracks)}] {artist} / {album} / {title}', flush=True)
            if output.exists() and not args.overwrite:
                print(f'  WAV exists, skipped: {output.name}', flush=True)
            else:
                with tempfile.NamedTemporaryFile(prefix='.dsimusic-', suffix='.wav', dir=folder, delete=False) as tmp:
                    temp_path = Path(tmp.name)
                try:
                    run(['ffmpeg', '-nostdin', '-v', 'error', '-y', '-i', str(path),
                         '-map', '0:a:0', '-vn', '-ar', '32768', '-ac', '2',
                         '-c:a', 'pcm_s16le', '-f', 'wav', str(temp_path)])
                    temp_path.replace(output)
                finally:
                    temp_path.unlink(missing_ok=True)
            if not any(t['file'] == output.name for t in entry['tracks']):
                entry['tracks'].append({'file': output.name, 'title': tags.get('title') or title,
                                        'track': int(track_no) if track_no else None,
                                        'source_format': path.suffix.lower().lstrip('.')})
            cover_path = folder / 'cover.bin'
            if not cover_path.exists():
                rgb = image_rgb(path, streams)
                if rgb:
                    write_cover(rgb, cover_path)
                    print('  Album artwork extracted', flush=True)
            if cover_path.exists():
                entry['cover'] = 'cover.bin'
        except (subprocess.CalledProcessError, OSError, ValueError, json.JSONDecodeError) as exc:
            failed += 1
            print(f'  ERROR: {path}: {exc}', file=sys.stderr)
    manifest = {'version': 1, 'audio': {'format': 'PCM WAV', 'sample_rate': 32768,
                                       'bit_depth': 16, 'channels': 2},
                'albums': sorted(albums.values(), key=lambda a: (a['artist'].casefold(), a['album'].casefold()))}
    for album in manifest['albums']:
        album['tracks'].sort(key=lambda t: (t['track'] if t['track'] is not None else 9999, t['file'].casefold()))

    # Track manifests for DSiMusic's FAT filesystem scanner.
    for entry in manifest['albums']:
        folder = target / entry['path']
        names = [track['file'] for track in entry['tracks']]
        (folder / 'tracks.txt').write_text(
            '\n'.join(names) + '\n',
            encoding='utf-8'
        )

    (root / 'library.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'\nDone: {len(manifest["albums"])} albums; {failed} failed files. Output: {root}')
    if failed:
        sys.exit(1)


if __name__ == '__main__':
    main()
