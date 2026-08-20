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
echo "==> done: $APP is signed, notarised and stapled"
