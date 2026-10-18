#!/bin/sh
# Converts the videos of the game (resource/videos/**/*.wmv) to H.264/AAC .mp4 next to them:
# on macOS and iOS the engine plays them with AVFoundation, which has no WMV decoder.
# usage: convert-videos.sh <game folder>   (needs ffmpeg: brew install ffmpeg)
set -e
[ -d "$1" ] || { echo "usage: $0 <game folder>"; exit 1; }
videos=$(find "$1" -maxdepth 2 -type d -iname videos -path '*/[Rr][Ee][Ss][Oo][Uu][Rr][Cc][Ee]/*' | head -1)
[ -n "$videos" ] || { echo "no resource/videos in $1"; exit 1; }
find "$videos" -type f -iname '*.wmv' | while read -r src; do
    dst="${src%.*}.mp4"
    [ -f "$dst" ] && continue
    echo "$src"
    ffmpeg -nostdin -loglevel error -i "$src" -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p \
        -c:a aac -b:a 192k -movflags +faststart "$dst.tmp.mp4"
    mv "$dst.tmp.mp4" "$dst"
done
