# Installation notes for macOS Catalina (v10.15)

_This document specifically refers to upgrades from previous versions of macOS to Catalina (10.15). It should be removed from the source repository when Catalina ceases to be the latest macOS version or updated to deal with challenges involved in upgrades to the next version of macOS._

Lessons learned from:
* https://github.com/nodejs/node-gyp/issues/1779
* https://github.com/nodejs/node-gyp/issues/1861
* https://github.com/nodejs/node-gyp/issues/1927 and elsewhere

Installing `node-gyp` on macOS can be found at https://github.com/nodejs/node-gyp#on-macos

However, upgrading to macOS Catalina changes some settings that may cause normal `node-gyp` installations to fail.

### Is my Mac running macOS Catalina?
Let's make first make sure that your Mac is currently running Catalina:
% `sw_vers`
    ProductName:	Mac OS X
    ProductVersion:	10.15
    BuildVersion:	19A602

If `ProductVersion` is less then `10.15` then this document is not really for you.

### The acid test
Next, lets see if `Xcode Command Line Tools` are installed:
1. `/usr/sbin/pkgutil --packages | grep CL`
    * If nothing is listed, then [skip to the next section](#Two-roads).
    * If `com.apple.pkg.CLTools_Executables` is listed then try:
2. `/usr/sbin/pkgutil --pkg-info com.apple.pkg.CLTools_Executables`
    * If `version: 11.0.0` or later is listed then _you are done_!  Your Mac should be ready to install `node-gyp`.  Doing `clang -v` should show `Apple clang version 11.0.0` or later.

As you go through the remainder of this document, at anytime you can try these `acid test` commands. If they pass then your Mac should be ready to install `node-gyp`.

### Two roads
There are two main ways to install `node-gyp` on macOS:
1. With the full Xcode (~7.6 GB download) from the `App Store` app.
2. With the _much_ smaller Xcode Command Line Tools via `xcode-select --install`

### Installing `node-gyp` using the full Xcode
1. `xcodebuild -version` should show `Xcode 11.1` or later.
    * If not, then install/upgrade Xcode from the App Store app.
2. Open the Xcode app and...
    * Under __Preferences > Locations__ select the tools if their location is empty.
    * Allow Xcode app to do an essential install of the most recent compiler tools.
3. Once all installations are _complete_, quit out of Xcode.
4. `sudo xcodebuild -license accept`  # If you agree with the licensing terms.
5. `softwareupdate -l`  # No listing is a good sign.
    * If Xcode or Tools upgrades are listed, use "Software Upgrade" to install them.
6. `xcode-select -version`  # Should return `xcode-select version 2370` or later.
7. `xcode-select -print-path`  # Should return `/Applications/Xcode.app/Contents/Developer`
8. Try the [_acid test_ steps above](#The-acid-test) to see if your Mac is ready.
9. If the _acid test_ does _not_ pass then...
10. `sudo xcode-select --reset`  # Enter root password.  No output is normal.
11. Repeat step 7 above.  Is the path different this time?  Repeat the _acid test_.

### Installing `node-gyp` using the Xcode Command Line Tools
1. If the _acid test_ has not succeeded, then try `xcode-select --install`
2. Wait until the install process is _complete_.
3. `softwareupdate -l`  # No listing is a good sign.
    * If Xcode or Tools upgrades are listed, use "Software Update" to install them.
4. `xcode-select -version`  # Should return `xcode-select version 2370` or later.
5. `xcode-select -print-path`  # Should return `/Library/Developer/CommandLineTools`
6. Try the [_acid test_ steps above](#The-acid-test) to see if your Mac is ready.
7. If the _acid test_ does _not_ pass then...
8. `sudo xcode-select --reset`  # Enter root password.  No output is normal.
9. Repeat step 5 above.  Is the path different this time?  Repeat the _acid test_.

### I did all that and the acid test still does not pass :-(
1. `sudo rm -rf $(xcode-select -print-path)`  # Enter root password.  No output is normal.
2. `xcode-select --install`
3. If the [_acid test_](#The-acid-test) still does _not_ pass then...
4. `npm explore npm -g -- npm install node-gyp@latest`
5. `npm explore npm -g -- npm explore npm-lifecycle -- npm install node-gyp@latest`
6. If the _acid test_ still does _not_ pass then...
7. Add a comment to https://github.com/nodejs/node-gyp/issues/1927 so we can improve.
