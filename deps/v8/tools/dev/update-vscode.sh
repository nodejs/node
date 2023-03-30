#!/bin/bash
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The purpose of this script is to make it easy to download/update
# Visual Studio Code on Linux distributions where for whatever reason there
# is no good way to do so via the package manager.

# Version of this script: 2022.11.12

# Basic checking of arguments: want at least one, and it's not --help.
VERSION="$1"
[ -z "$VERSION" -o \
    "$VERSION" == "-h" -o \
    "$VERSION" == "--help" -o \
    "$VERSION" == "help" ] && {
  echo "Usage: $0 <version>"
  echo "<version> may be --auto for auto-detecting the latest available."
  exit 1
}

die() {
  echo "Error: $1"
  exit 1
}

if [ "$VERSION" == "--auto" -o "$VERSION" == "auto" ]; then
  echo "Searching online for latest available version..."
  # Where to find the latest available version.
  AVAILABLE_PACKAGES_URL="https://packages.microsoft.com/repos/vscode/dists/stable/main/binary-amd64/Packages.gz"
  VERSION=$(curl "$AVAILABLE_PACKAGES_URL" --silent \
            | gunzip \
            | gawk '
              BEGIN { engaged = 0 }
              # Look at blocks starting with "Package: code".
              /^Package: code$/ { engaged = 1 }
              # Stop looking at the empty line indicating the end of a block.
              /^$/ { engaged = 0 }
              # In interesting blocks, print the relevant part of the
              # "Version: " line.
              match($0, /^Version: ([0-9.]*)/, groups) {
                if (engaged == 1) print groups[1]
              }
              ' - \
            | sort -rV \
            | head -1)
  if [ -z "$VERSION" ]; then
    die "Detecting latest version failed, please specify it manually."
  else
    echo "Latest version found: $VERSION"
  fi
fi

# Constant definitions for local paths. Edit these to your liking.
VSCODE_DIR="$HOME/vscode"
BACKUP_DIR="$HOME/vscode.prev"
DOWNLOADS_DIR="$HOME/Downloads"
DOWNLOAD_FILE="$DOWNLOADS_DIR/vscode-$VERSION.tar.gz"
DESKTOP_FILE_DIR="$HOME/.local/share/applications"
DESKTOP_FILE="$DESKTOP_FILE_DIR/code.desktop"

# Constant definitions for remote/upstream things. Might need to be updated
# when upstream changes things.
# Where to find the version inside VS Code's installation directory.
PACKAGE_JSON="$VSCODE_DIR/resources/app/package.json"
ICON="$VSCODE_DIR/resources/app/resources/linux/code.png"
# Where to download the archive.
DOWNLOAD_URL="https://update.code.visualstudio.com/$VERSION/linux-x64/stable"
CODE_BIN="$VSCODE_DIR/bin/code"

# Check for "code" in $PATH; create a symlink if we can find a good place.
SYMLINK=$(which code)
if [ -z "$SYMLINK" ]; then
  IFS=':' read -ra PATH_ARRAY <<< "$PATH"
  for P in "${PATH_ARRAY[@]}"; do
    if [ "$P" == "$HOME/bin" -o \
         "$P" == "$HOME/local/bin" -o \
         "$P" == "$HOME/.local/bin" ]; then
      LOCALBIN="$P"
      break
    fi
  done
  if [ -n "$LOCALBIN" ]; then
    echo "Adding symlink to $LOCALBIN..."
    if [ ! -d "$LOCALBIN" ]; then
      mkdir -p "$LOCALBIN" || die "Failed to create $LOCALBIN."
    fi
    ln -s "$CODE_BIN" "$LOCALBIN/code" || die "Failed to create symlink."
  else
    echo "Please put a symlink to $CODE_BIN somewhere on your \$PATH."
  fi
fi

if [ ! -r "$DESKTOP_FILE" ]; then
  echo "Creating .desktop file..."
  mkdir -p "$DESKTOP_FILE_DIR" || die "Failed to create .desktop directory."
  cat <<EOF > "$DESKTOP_FILE"
#!/usr/bin/env xdg-open
[Desktop Entry]
Name=Visual Studio Code
Comment=Code Editing. Redefined.
GenericName=Text Editor
Exec=$CODE_BIN --unity-launch %F
Icon=$ICON
Type=Application
StartupNotify=false
StartupWMClass=Code
Categories=Utility;TextEditor;Development;IDE;
MimeType=text/plain;inode/directory;
Actions=new-empty-window;
Keywords=vscode;

X-Desktop-File-Install-Version=0.24

[Desktop Action new-empty-window]
Name=New Empty Window
Exec=$CODE_BIN --new-window %F
Icon=$ICON
EOF
  chmod +x "$DESKTOP_FILE" || die "Failed to make .desktop file executable."
fi

# Find currently installed version.
if [ -d "$VSCODE_DIR" ]; then
  if [ ! -r "$PACKAGE_JSON" ]; then
    die "$PACKAGE_JSON file not found, this script must be updated."
  fi
  INSTALLED=$(grep '"version":' "$PACKAGE_JSON" \
              | sed 's/[^0-9]*\([0-9.]*\).*/\1/')
  echo "Detected installed version: $INSTALLED"
  if [ "$VERSION" == "$INSTALLED" ] ; then
    echo "You already have that version."
    exit 0
  else
    echo "Updating from $INSTALLED to $VERSION..."
  fi
fi

if [ ! -r "$DOWNLOAD_FILE" ]; then
  echo "Downloading..."
  if [ ! -d "$DOWNLOADS_DIR" ]; then
    mkdir -p "$DOWNLOADS_DIR" || die "Failed to create $DOWNLOADS_DIR."
  fi
  wget "$DOWNLOAD_URL" -O "$DOWNLOAD_FILE" || die "Downloading failed."
else
  echo "$DOWNLOAD_FILE already exists; delete it to force re-download."
fi

echo "Extracting..."
TAR_DIR=$(tar -tf "$DOWNLOAD_FILE" | head -1)
[ -z "$TAR_DIR" ] && die "Couldn't read archive."
TMP_DIR=$(mktemp -d)
tar -C "$TMP_DIR" -xf "$DOWNLOAD_FILE" || {
  rm -rf "$TMP_DIR"
  die "Extracting failed."
}

if [ -d "$BACKUP_DIR" ]; then
  echo "Deleting previous backup..."
  rm -rf "$BACKUP_DIR"
fi

if [ -d "$VSCODE_DIR" ]; then
  echo "Moving previous installation..."
  mv "$VSCODE_DIR" "$BACKUP_DIR"
fi

echo "Installing new version..."
mv "$TMP_DIR/$TAR_DIR" "$VSCODE_DIR"
rmdir "$TMP_DIR"

echo "All done, enjoy coding!"
