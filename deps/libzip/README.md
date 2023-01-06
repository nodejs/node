This is libzip, a C library for reading, creating, and modifying
zip and zip64 archives. Files can be added from data buffers, files,
or compressed data copied directly from other zip archives. Changes
made without closing the archive can be reverted. Decryption and
encryption of Winzip AES and legacy PKware encrypted files is
supported.

libzip is fully documented via man pages. HTML versions of the man
pages are on [libzip.org](https://libzip.org/documentation/) and
in the [man](man) directory. You can start with
[libzip(3)](https://libzip.org/documentation/libzip.html), which
lists
all others. Example source code is in the [examples](examples) and
[src](src) subdirectories.

See the [INSTALL.md](INSTALL.md) file for installation instructions and
dependencies.

If you have developed an application using libzip, you can find out
about API changes and how to adapt your code for them in the included
file [API-CHANGES.md](API-CHANGES.md).

If you make a binary distribution, please include a pointer to the
distribution site:
>	https://libzip.org/

The latest version can always be found there.  The official repository
is at [github](https://github.com/nih-at/libzip/).

If you want to reach the authors in private, use <info@libzip.org>.

[![Github Actions Build Status](https://github.com/nih-at/libzip/workflows/build/badge.svg)](https://github.com/nih-at/libzip/actions?query=workflow%3Abuild)
[![Appveyor Build status](https://ci.appveyor.com/api/projects/status/f1bqqt9djvf22f5g?svg=true)](https://ci.appveyor.com/project/nih-at/libzip)
[![Coverity Status](https://scan.coverity.com/projects/127/badge.svg)](https://scan.coverity.com/projects/libzip)
