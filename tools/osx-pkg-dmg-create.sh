#!/bin/bash
# Create a complete OS .dmg file (it needs the Apple Developers Tools installed)
# usage:
#    pkg-create.sh <contents-root-folder> <package-name> <package-version> <vendor-string>
#

CONTENTS=$1
shift
NAME=$1
shift
VERSION=$1
shift
VENDOR=$1

PKGID="$VENDOR.$NAME-$VERSION"

# unused pkg-info entries so far
#
#    <key>CFBundleExecutable</key>
#    <string>$NAME</string>
#    <key>CFBundleSignature</key>
#    <string>????</string>

#
# Need the .plist file in order for the packagemaker to create a package which the
# pkgutil --packages later on would return an entry about. pkgutil can then --unlink
# and --forget about the package nicely.
#
cat > "$CONTENTS.plist" <<PINFO
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<pkg-info version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleIdentifier</key>
    <string>$PKGID</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
</dict>
</pkg-info>
PINFO

packagemaker=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker

$packagemaker \
    --id "$PKGID" \
    --info "$CONTENTS.plist" \
    --root "$CONTENTS" \
    --target 10.5 \
    --out "$CONTENTS".pkg

hdiutil create "$CONTENTS.dmg" \
    -format UDZO -ov \
    -volname "$NAME $VERSION" \
    -srcfolder $CONTENTS.pkg

