#!/bin/sh
# Regenerates the installer logo assets in doc/ from the standardized
# Node.js logo artwork (https://github.com/openjs-foundation/artwork).
#
# The generated images are consumed by:
#   - tools/msvs/msi/nodemsi/product.wxs           (Windows MSI banner/dialog)
#   - tools/macos-installer/productbuild/distribution.xml.tmpl
#                                                  (macOS installer background)
#
# The WiX UI bitmap slots are fixed-size (493x58 banner, 493x312 dialog) and
# the macOS background is placed unscaled (180x361), so the output dimensions
# must not change. Logo widths and offsets below were measured from the
# previous assets so the layout stays identical.
#
# Prerequisites: rsvg-convert (librsvg), magick (ImageMagick 7), curl
#
# Usage: tools/generate-installer-assets.sh

set -e

# Pinned artwork revision so the output is reproducible.
ARTWORK_REF=3816245ebbf4707bdafde748350ac8476b6b5b62
ARTWORK_URL="https://raw.githubusercontent.com/openjs-foundation/artwork/$ARTWORK_REF/projects/nodejs"

# Node.js brand green, matching the solid fill used in the standardized logo
# (see https://nodejs.org/en/about/branding).
GREEN='#5FA04E'

DOC_DIR="$(dirname "$0")/../doc"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

curl -sSfL "$ARTWORK_URL/nodejs-logo-stacked-color.svg" \
  -o "$TMP_DIR/logo-light.svg"
curl -sSfL "$ARTWORK_URL/nodejs-logo-stacked-color-dark_background.svg" \
  -o "$TMP_DIR/logo-dark.svg"

# out             canvas   stripe logo_w x   y    background logo
MANIFEST='
thin-white-stripe.jpg        493x58  6 80  380 7   white light
full-white-stripe.jpg        493x312 6 116 37  89  white light
osx_installer_logo.png       180x361 0 142 19  254 none  light
osx_installer_logo_dark.png  180x361 0 142 19  254 none  dark
'

echo "$MANIFEST" | while read -r out canvas stripe logo_w x y bg logo; do
  [ -z "$out" ] && continue

  rsvg-convert -w "$logo_w" "$TMP_DIR/logo-$logo.svg" -o "$TMP_DIR/logo.png"

  width="${canvas%x*}"
  if [ "$stripe" -gt 0 ]; then
    draw="rectangle 0,0 $width,$((stripe - 1))"
  else
    draw=''
  fi

  magick -size "$canvas" "canvas:$bg" \
    -fill "$GREEN" ${draw:+-draw "$draw"} \
    "$TMP_DIR/logo.png" -geometry "+$x+$y" -composite \
    -strip -quality 95 "$DOC_DIR/$out"

  got="$(magick identify -format '%wx%h' "$DOC_DIR/$out")"
  if [ "$got" != "$canvas" ]; then
    echo "error: $out is $got, expected $canvas" >&2
    exit 1
  fi
  echo "generated doc/$out ($got)"
done
