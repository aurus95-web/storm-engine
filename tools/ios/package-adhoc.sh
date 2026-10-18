#!/bin/sh
# Packages an unsigned iOS build (CODE_SIGNING_ALLOWED=NO) into an ad-hoc signed IPA for
# jailbroken devices with AppSync Unified or TrollStore; Xcode refuses ad-hoc signing for iOS.
#
# Usage: tools/ios/package-adhoc.sh [--data <game folder> [--ini <engine.ini>]] [--sign <identity>] <engine.app> <output.ipa|output.app> [icon.png [display name]]
# The icon (a square picture; tools/ios/make_icon.py draws one without the art of the game) is resized for the iPad.
# With --data the IPA carries the game too (in GameData of the app, without the Windows files, the WMV videos, the logs
# and the saved games): the engine puts it into the Documents folder at the first start of every new data version,
# keeping the engine.ini and options of the player. --ini replaces the engine.ini of the game folder (for the iPad).
# --sign signs with a development certificate instead (devices without a jailbreak) and keeps the entitlements and the
# provisioning profile of the app built by Xcode with automatic signing; an output ending with .app is the app itself
# (for xcrun devicectl device install app).
set -e

data=""
ini=""
identity=""
while [ $# -gt 0 ]; do
    case "$1" in
    --data) data="$2"; shift 2 ;;
    --ini) ini="$2"; shift 2 ;;
    --sign) identity="$2"; shift 2 ;;
    *) break ;;
    esac
done
if [ -n "$data" ] && [ ! -d "$data/PROGRAM" ] && [ ! -d "$data/program" ]; then
    echo "no game in $data" >&2
    exit 1
fi

app="$1"
ipa="$2"
icon="$3"
name="$4"
if [ ! -d "$app" ] || [ -z "$ipa" ]; then
    echo "usage: $0 <engine.app> <output.ipa> [icon.png [display name]]" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/Payload"
cp -R "$app" "$work/Payload/"
bundle="$work/Payload/$(basename "$app")"

if [ -n "$icon" ]; then
    plist="$bundle/Info.plist"
    sips -z 152 152 "$icon" --out "$bundle/AppIcon76x76@2x~ipad.png" >/dev/null
    sips -z 167 167 "$icon" --out "$bundle/AppIcon83.5x83.5@2x~ipad.png" >/dev/null
    sips -z 120 120 "$icon" --out "$bundle/AppIcon60x60@2x.png" >/dev/null
    /usr/libexec/PlistBuddy -c "Delete :CFBundleIcons" -c "Delete :CFBundleIcons~ipad" "$plist" 2>/dev/null || true
    for key in CFBundleIcons CFBundleIcons~ipad; do
        /usr/libexec/PlistBuddy -c "Add :$key:CFBundlePrimaryIcon:CFBundleIconFiles array" \
            -c "Add :$key:CFBundlePrimaryIcon:CFBundleIconFiles:0 string AppIcon60x60" "$plist"
    done
    /usr/libexec/PlistBuddy -c "Add :CFBundleIcons~ipad:CFBundlePrimaryIcon:CFBundleIconFiles:1 string AppIcon76x76" \
        -c "Add :CFBundleIcons~ipad:CFBundlePrimaryIcon:CFBundleIconFiles:2 string AppIcon83.5x83.5" "$plist"
fi
if [ -n "$name" ]; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName $name" "$bundle/Info.plist"
fi

if [ -n "$data" ]; then
    rsync -a --exclude '*.wmv' --exclude '*.log' --exclude '*.exe' --exclude '*.dll' --exclude 'd3d8.ini' \
        --exclude '*.dxvk-cache' --exclude 'engine.ini.*' --exclude 'SAVE/' --exclude 'uninstall/' \
        --exclude '.gamedata_version' "$data/" "$bundle/GameData/"
    [ -n "$ini" ] && cp "$ini" "$bundle/GameData/engine.ini"
    # the version is of the content: the data is put into Documents again only when it changes
    (cd "$bundle/GameData" && find . -type f -exec stat -f "%N %z %m" {} + | LC_ALL=C sort | md5) >"$bundle/GameData/.version"
fi

if [ -n "$identity" ]; then
    entitlements="$work/app.entitlements"
    codesign -d --entitlements :- "$app" >"$entitlements" 2>/dev/null
    for lib in "$bundle"/Frameworks/*.dylib "$bundle"/Frameworks/*.framework; do
        [ -e "$lib" ] && codesign -f -s "$identity" "$lib"
    done
    codesign -f -s "$identity" --entitlements "$entitlements" "$bundle"
else
    for lib in "$bundle"/Frameworks/*.dylib "$bundle"/Frameworks/*.framework; do
        [ -e "$lib" ] && codesign -f -s - "$lib"
    done
    codesign -f -s - --entitlements "$(dirname "$0")/adhoc.entitlements" "$bundle"
fi

ipa_path=$(cd "$(dirname "$ipa")" && pwd)/$(basename "$ipa")
rm -rf "$ipa_path"
case "$ipa_path" in
*.app) mv "$bundle" "$ipa_path" ;;
*) (cd "$work" && zip -qry "$ipa_path" Payload) ;;
esac
echo "$ipa_path"
