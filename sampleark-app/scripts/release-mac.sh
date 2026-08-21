#!/usr/bin/env bash
# Sign, notarise and staple the macOS build for distribution.
#
# Needs two things this repo cannot hold:
#   1. A "Developer ID Application" certificate in the login keychain.
#      Check with: security find-identity -v -p codesigning
#      Override the auto-detected one with: export SAMPLEARK_SIGN_ID="Developer ID Application: NAME (TEAMID)"
#   2. Notary credentials stored once, interactively:
#      xcrun notarytool store-credentials "SampleArk" --apple-id you@example.com --team-id TEAMID
#      (it prompts for an app-specific password; use --key/--key-id/--issuer for an App Store Connect key)
#      Override the profile name with: export SAMPLEARK_NOTARY_PROFILE=...
#
# Usage: scripts/release-mac.sh [path/to/SampleArk.app]
set -euo pipefail

cd "$(dirname "$0")/.."
APP="${1:-build-release/app/SampleArk_artefacts/Release/SampleArk.app}"
PROFILE="${SAMPLEARK_NOTARY_PROFILE:-SampleArk}"
ENTITLEMENTS="scripts/SampleArk.entitlements"
ZIP="${APP%.app}-notarize.zip"

die() { printf '\nerror: %s\n' "$1" >&2; exit 1; }

[ -d "$APP" ] || die "no app bundle at $APP — build it first: cmake --build build-release --target SampleArk -j"

IDENTITY="${SAMPLEARK_SIGN_ID:-}"
if [ -z "$IDENTITY" ]; then
  IDENTITY=$(security find-identity -v -p codesigning 2>/dev/null \
             | grep "Developer ID Application" | head -1 | sed -E 's/.*"(.*)"/\1/') || true
fi
[ -n "$IDENTITY" ] || die "no Developer ID Application identity found. Import the certificate into the login
       keychain (export it as .p12 from the Mac that has it, or make a new one from a CSR in the
       developer portal), or set SAMPLEARK_SIGN_ID to the identity string."

echo "==> signing as: $IDENTITY"
# Inside-out: anything embedded is signed before the bundle that contains it.
if [ -d "$APP/Contents/Frameworks" ]; then
  find "$APP/Contents/Frameworks" -type f \( -name "*.dylib" -o -name "*.so" \) -print0 \
    | while IFS= read -r -d '' lib; do
        codesign --force --timestamp --options runtime --sign "$IDENTITY" "$lib"
      done
  find "$APP/Contents/Frameworks" -maxdepth 1 -type d -name "*.framework" -print0 \
    | while IFS= read -r -d '' fw; do
        codesign --force --timestamp --options runtime --sign "$IDENTITY" "$fw"
      done
fi
codesign --force --timestamp --options runtime \
         --entitlements "$ENTITLEMENTS" --sign "$IDENTITY" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

echo "==> submitting for notarisation (profile: $PROFILE)"
rm -f "$ZIP"
/usr/bin/ditto -c -k --keepParent "$APP" "$ZIP"        # ditto, not zip: preserves the bundle
xcrun notarytool submit "$ZIP" --keychain-profile "$PROFILE" --wait

echo "==> stapling"
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"

echo "==> verifying as Gatekeeper sees it"
spctl --assess --type exec -vvv "$APP"
codesign --display --verbose=4 "$APP" 2>&1 | grep -E "Identifier|TeamIdentifier|Timestamp|Runtime" || true

rm -f "$ZIP"

# ---- package ----------------------------------------------------------------
# Assembled here rather than by hand, so the guide that ships can never drift
# from the one in the repo (it has: a stale copy once told a tester the app
# needed an OS it doesn't).
STAMP=$(stat -f "%Sm" -t "%Y-%m-%d_%H%M" "$APP/Contents/MacOS/SampleArk")
ARCHS=$(lipo -archs "$APP/Contents/MacOS/SampleArk")
case "$ARCHS" in
  *x86_64*arm64*|*arm64*x86_64*) LABEL="universal" ;;
  *)                             LABEL="${ARCHS// /-}" ;;
esac
OUT="dist/SampleArk-$LABEL-$STAMP"

echo "==> packaging $OUT"
rm -rf "$OUT" "$OUT.zip"
mkdir -p "$OUT"
/usr/bin/ditto "$APP" "$OUT/SampleArk.app"          # ditto: keeps signature + ticket
cp USER_GUIDE.txt "$OUT/SampleArk User Guide.txt"
touch "$OUT/SampleArk.app" "$OUT"                    # so Finder shows the build date
/usr/bin/ditto -c -k --keepParent "$OUT" "$OUT.zip"

# ---- verify the package, not the thing we just built ------------------------
# Unpack it somewhere clean and check what a recipient actually gets.
VERIFY=$(mktemp -d)
trap 'rm -rf "$VERIFY"' EXIT
/usr/bin/ditto -x -k "$OUT.zip" "$VERIFY"
GOT="$VERIFY/$(basename "$OUT")"
spctl --assess --type exec "$GOT/SampleArk.app" >/dev/null 2>&1   || die "the packaged app does not pass Gatekeeper"
xcrun stapler validate "$GOT/SampleArk.app" >/dev/null 2>&1   || die "the packaged app has no valid stapled ticket"
cmp -s USER_GUIDE.txt "$GOT/SampleArk User Guide.txt"   || die "the packaged guide differs from USER_GUIDE.txt"

echo
echo "    architectures : $ARCHS"
echo "    minimum macOS : $(otool -l "$GOT/SampleArk.app/Contents/MacOS/SampleArk" | awk '/minos/{print $2; exit}')"
echo "    gatekeeper    : $(spctl --assess --type exec -vv "$GOT/SampleArk.app" 2>&1 | awk -F= '/source=/{print $2}')"
echo "    guide         : matches USER_GUIDE.txt"
echo
echo "==> send this:"
echo "    $PWD/$OUT.zip"
