# Changelog

## 6.0

- Drop support for node 6 and 8
- fix symlinks and hardlinks on windows being packed with `\`-style path
  targets

## 5.0

- Address unpack race conditions using path reservations
- Change large-numbers errors from TypeError to Error
- Add `TAR_*` error codes
- Raise `TAR_BAD_ARCHIVE` warning/error when there are no valid entries
  found in an archive
- do not treat ignored entries as an invalid archive
- drop support for node v4
- unpack: conditionally use a file mapping to write files on Windows
- Set more portable 'mode' value in portable mode
- Set `portable` gzip option in portable mode

## 4.4

- Add 'mtime' option to tar creation to force mtime
- unpack: only reuse file fs entries if nlink = 1
- unpack: rename before unlinking files on Windows
- Fix encoding/decoding of base-256 numbers
- Use `stat` instead of `lstat` when checking CWD
- Always provide a callback to fs.close()

## 4.3

- Add 'transform' unpack option

## 4.2

- Fail when zlib fails

## 4.1

- Add noMtime flag for tar creation

## 4.0

- unpack: raise error if cwd is missing or not a dir
- pack: don't drop dots from dotfiles when prefixing

## 3.1

- Support `@file.tar` as an entry argument to copy entries from one tar
  file to another.
- Add `noPax` option
- `noResume` option for tar.t
- win32: convert `>|<?:` chars to windows-friendly form
- Exclude mtime for dirs in portable mode

## 3.0

- Minipass-based implementation
- Entirely new API surface, `tar.c()`, `tar.x()` etc., much closer to
  system tar semantics
- Massive performance improvement
- Require node 4.x and higher

## 0.x, 1.x, 2.x - 2011-2014

- fstream-based implementation
- slow and kinda bad, but better than npm shelling out to the system `tar`
