## Before next release

## Other

- split `zip_source_t` in main part and reference so we can keep track which reference called open and we can invalidate references if the underlying source gets invalidated (e. g. by `zip_close`). 
- Support extended timestamp extra field (0x5455): mtime overrides dos mtime from dirent, function to get/set all three.  
- Add support for torrentzip.

## Prefixes

For example for adding extractors for self-extracting zip archives.
````c
zip_set_archive_prefix(struct zip *za, const zip_uint8_t *data, zip_uint64_t length);
const zip_uint8_t *zip_get_archive_prefix(struct zip *za, zip_uint64_t *lengthp);
````

## Compression

* add lzma2 support
* add deflate64 support (https://github.com/madler/zlib/blob/master/contrib/infback9/infback9.h)

## API Issues

* Add `zip_file_use_password` to set per-file password to use if libzip needs to decrypt the file (e.g. when changing encryption or compression method).

* `zip_get_archive_comment` has `int *lenp` argument.  Cleaner would be `zip_uint32_t *`.
  rename and fix.  which other functions for naming consistency?
* rename remaining `zip_XXX_{file,archive}_*` to `zip_{file,archive}_XXX_*`?
* compression/crypt implementations: how to set error code on failure
* compression/crypt error messages a la `ZIP_ER_ZLIB` (no detailed info passing)

## Features

* consistently use `_zip_crypto_clear()` for passwords
* support setting extra fields from `zip_source`
  * introduce layers of extra fields:
    * original
    * from `zip_source`
    * manually set
  * when querying extra fields, search all of them in reverse order
  * add whiteout (deleted) flag
  * allow invalid data flag, used when computing extra field size before writing data
  * new command `ZIP_SOURCE_EXTRA_FIELDS`
  * no support for multiple copies of same extra field
* delete all extra fields during `zip_replace()`
* function to copy file from one archive to another
* set `O_CLOEXEC` flag after fopen and mkstemp
* `zip_file_set_mtime()`: support InfoZIP time stamps
* support streaming output (creating new archive to e.g. stdout)
* add function to read/set ASCII file flag
* `zip_commit()` (to finish changes without closing archive)
* add custom compression function support
* `zip_source_zip()`: allow rewinding
* `zipcmp`: add option for file content comparison
* `zipcmp`: add more paranoid checks:
  * external attributes/opsys
  * last_mod
  * version needed/made by
  * general purpose bit flags
* add more consistency checks:
  * for stored files, test compressed = uncompressed
  * data descriptor
  * local headers come before central dir
* support for old compression methods?

## Bugs

* ensure that nentries is small enough not to cause overflow (size_t for entry, uint64 for CD on disk)
* check for limits imposed by format (central dir size, file size, extra fields, ...)
* `_zip_u2d_time()`: handle `localtime(3)` failure
* POSIX: `zip_open()`: check whether file can be created and fail if not
* fix inconsistent usage of valid flags (not checked in many places)
* `cdr == NULL` -> `ER_NOENT` vs. `idx > cdir->nentry` -> `ER_INVAL` inconsistent (still there?)

## Cleanup

* go over cdir parser and rename various offset/size variables to make it clearer
* use bool
* use `ZIP_SOURCE_SUPPORTS_{READABLE,SEEKABLE,WRITABLE}`
* use `zip_source_seek_compute_offset()`
* get rid of `zip_get_{compression,encryption}_implementation()`
* use `zip_*int*_t` internally
* `zip_source_file()`: don't allow write if start/len specify a part of the file

## Documentation

* document valid file paths
* document: `zip_source_write()`: length can't be > `ZIP_INT64_MAX`
* document: `ZIP_SOURCE_CLOSE` implementation can't return error
* keep error codes in man pages in sync
* document error codes in new man pages

## Infrastructure

* review guidelines/community standards
  - [Linux Foundation Core Infrastructure Initiative Best Practices](https://bestpractices.coreinfrastructure.org/)
  - [Readme Maturity Level](https://github.com/LappleApple/feedmereadmes/blob/master/README-maturity-model.md)
  - [Github Community Profile](https://github.com/nih-at/libzip/community)
* test different crypto backends with GitHub actions.
* improve man page formatting of tagged lists on webpage (`<dl>`)
* rewrite `make_zip_errors.sh` in cmake
* script to check if all exported symbols are marked with `ZIP_EXTERN`, add to `make distcheck`

## macOS / iOS framework

* get cmake to optionally build frameworks

## Test Case Issues

* add test cases for all ZIP_INCONS detail errors
* `incons-local-filename-short.zzip` doesn't test short filename, since extra fields fail to parse.
* test error cases with special source
  - tell it which command should fail
  - use it both as source for `zip_add` and `zip_open_from_source`
  - `ziptool_regress`:
    - `-e error_spec`: source containing zip fails depending on `error_spec`
    - `add_with_error name content error_spec`: add content to archive, where source fails depending on `error_spec`
    - `add_file_with_error name file_to_add offset len error_spec`: add file to archive, len bytes starting from offset, where source fails depending on `error_spec`
  - `error_spec`:
    - source command that fails
	- error code that source returns
	- conditions that must be met for error to trigger
	  - Nth call of command
      - read/write: total byte count so far
	  - state of source (opened, EOF reached, ...)
* test for zipcmp reading directory (requires fts)
* add test case for clone with files > 4k
* consider testing for malloc/realloc failures
* Winzip AES support
  * test cases decryption: <=20, >20, stat for both
  * test cases encryption: no password, default password, file-specific password, 128/192/256, <=20, >20
  * support testing on macOS
* add test cases for lots of files (including too many)
* add test cases for holes (between files, between files and cdir, between cdir and eocd, + zip64 where appropriate)
* test seek in `zip_source_crc_create()`
* test cases for `set_extra*`, `delete_extra*`, `*extra_field*`
* test cases for in memory archives
  * add
  * delete
  * delete all
  * modify
* use gcov output to increase test coverage
* add test case to change values for newly added files (name, compression method, comment, mtime, . . .)
* `zip_open()` file less than `EOCDLEN` bytes long
* test calls against old API
* rename file to dir/ and vice versa (fails)
* fix comment test to be newline insensitive
* check if http://bugs.python.org/issue20078 provides ideas for new tests

* (`add`, `replace`)
  * add to empty zip
  * add to existing zip
  * add w/ existing file name [E]
  * replace ok
  * replace w/ illegal index [E]
  * replace w/ deleted name [E]
  * unchange added/replaced file
* (`close`)
  * copy zip file
  * open copy
  * rename, delete, replace, add w/ new name, add w/ deleted name
  * close
  * zipcmp copy expected
  * remove copy
* (`error_get)
* (`error_get_sys_type`)
* (`error_to_str`)
* (`extra_fields`)
* (`file_error_get`)
* (`file_strerror`)
* (`replace`)
* (`source_buffer`)
* (`source_file`)
* (`source_filep`)
* (`source_free`)
* (`source_function`)
* (`source_zip`)
* (`strerror`)
* (`unchange`)
* (`unchange_all`)
* `open(ZIP_RDONLY)`
* I/O abstraction layer
  * `zip_open_from_source`
* read two zip entries interleaved
