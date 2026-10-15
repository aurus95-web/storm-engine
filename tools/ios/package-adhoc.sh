#!/bin/sh
# Packages an unsigned iOS build (CODE_SIGNING_ALLOWED=NO) into an ad-hoc signed IPA for
# jailbroken devices with AppSync Unified or TrollStore; Xcode refuses ad-hoc signing for iOS.
#
# Usage: tools/ios/package-adhoc.sh <engine.app> <output.ipa>
set -e

app="$1"
ipa="$2"
if [ ! -d "$app" ] || [ -z "$ipa" ]; then
    echo "usage: $0 <engine.app> <output.ipa>" >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/Payload"
cp -R "$app" "$work/Payload/"
bundle="$work/Payload/$(basename "$app")"

for lib in "$bundle"/Frameworks/*.dylib "$bundle"/Frameworks/*.framework; do
    [ -e "$lib" ] && codesign -f -s - "$lib"
done
codesign -f -s - --entitlements "$(dirname "$0")/adhoc.entitlements" "$bundle"

ipa_path=$(cd "$(dirname "$ipa")" && pwd)/$(basename "$ipa")
rm -f "$ipa_path"
(cd "$work" && zip -qry "$ipa_path" Payload)
echo "$ipa_path"
