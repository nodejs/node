# 1.10 [Unreleased]

* Make support for layered sources public.
* Allow reading changed file data.
* Fix handling of files of size 4294967295.
* `zipmerge`: copy extra fields.
* `zipmerge`: add option to keep files uncompressed.
* CMake: add detection of mbedTLS version 3.
* Use ISO C secure library functions, if available.
* Fix reading/writing compressed data with buffers > 4GiB.


# 1.9.2 [2022-06-28]

* Fix version number in header file.


# 1.9.1 [2022-06-28]

* Fix `zip_file_is_seekable()`.


# 1.9.0 [2022-06-13]

* Add `zip_file_is_seekable()`.
* Improve compatibility with WinAES.
* Fix encoding handling in `zip_name_locate()`.
* Add option to `zipcmp` to output summary of changes.
* Various bug fixes and documentation improvements.


# 1.8.0 [2021-06-18]

* Add support for zstd (Zstandard) compression.
* Add support for lzma (ID 14) compression.
* Add `zip_source_window_create()`.
* Add `zip_source_zip_create()` variant to `zip_source_zip()`.
* Allow method specific `comp_flags` in `zip_set_file_compression()`.
* Allow `zip_source_tell()` on sources that don't support seeking and `zip_ftell()` on compressed data.
* Provide more details for consistency check errors.
* Improve output of `zipcmp`.
* In `zipcmp`, don’t ignore empty directories when comparing directory listing.
* Treat empty string as no password given in `zip_file_set_encryption()`, `zip_fopen_encrypted()`, and `zip_set_default_password()`.


# 1.7.3 [2020-07-15]

* Support cmake < 3.17 again.
* Fix pkgconfig file (regression in 1.7.2).


# 1.7.2 [2020-07-11]

* Fixes for the CMake `find_project()` files.
* libzip moved to the CMake `libzip::` `NAMESPACE`.
* CMake usage best practice cleanups.


# 1.7.1 [2020-06-13]

* Restore `LIBZIP_VERSION_{MAJOR,MINOR,MICRO}` symbols.
* Fixes warnings reported by PVS-Studio.
* Add `LIBZIP_DO_INSTALL` build setting to make it easier to use
  libzip as subproject.


# 1.7.0 [2020-06-05]

* Add support for encrypting using traditional PKWare encryption.
* Add `zip_compression_method_supported()`.
* Add `zip_encryption_method_supported()`.
* Add the `ZIP_SOURCE_GET_FILE_ATTRIBUTES` source command.
* Refactor stdio file backend.
* Add CMake find_project() support.


# 1.6.1 [2020-02-03]

* Bugfix for double-free in `zipcmp(1)` during cleanup.


# 1.6.0 [2020-01-24]

* Avoid using `umask()` since it's not thread-safe.
* Set close-on-exec flag when opening files.
* Do not accept empty files as valid zip archives any longer.
* Add support for XZ compressed files (using liblzma).
* Add support for cancelling while closing zip archives.
* Add support for setting the time in the on-disk format.


# 1.5.2 [2019-03-12]

* Fix bug in AES encryption affecting certain file sizes
* Keep file permissions when modifying zip archives
* Support systems with small stack size.
* Support mbed TLS as crypto backend.
* Add nullability annotations.


# 1.5.1 [2018-04-11]

* Choose format of installed documentation based on available tools.
* Fix visibility of symbols.
* Fix zipcmp directory support.
* Don't set RPATH on Linux.
* Use Libs.private for link dependencies in pkg-config file.
* Fix build with LibreSSL.
* Various bugfixes.


# 1.5.0 [2018-03-11]

* Use standard cryptographic library instead of custom AES implementation.
  This also simplifies the license.
* Use `clang-format` to format the source code.
* More Windows improvements.


# 1.4.0 [2017-12-29]

* Improve build with cmake
* Retire autoconf/automake build system
* Add `zip_source_buffer_fragment()`.
* Add support to clone unchanged beginning of archive (instead of rewriting it).
  Supported for buffer sources and on Apple File System.
* Add support for Microsoft Universal Windows Platform.


# 1.3.2 [2017-11-20]

* Fix bug introduced in last: zip_t was erroneously freed if zip_close() failed.


# 1.3.1 [2017-11-19]

* Install zipconf.h into ${PREFIX}/include
* Add zip_libzip_version()
* Fix AES tests on Linux


# 1.3.0 [2017-09-02]

* Support bzip2 compressed zip archives
* Improve file progress callback code
* Fix zip_fdopen()
* CVE-2017-12858: Fix double free()
* CVE-2017-14107: Improve EOCD64 parsing


# 1.2.0 [2017-02-19]

* Support for AES encryption (Winzip version), both encryption
  and decryption
* Support legacy zip files with >64k entries
* Fix seeking in zip_source_file if start > 0
* Add zip_fseek() for seeking in uncompressed data
* Add zip_ftell() for telling position in uncompressed data
* Add zip_register_progress_callback() for UI updates during zip_close()


# 1.1.3 [2016-05-28]

* Fix build on Windows when using autoconf


# 1.1.2 [2016-02-19]

* Improve support for 3MF files


# 1.1.1 [2016-02-07]

* Build fixes for Linux
* Fix some warnings reported by PVS-Studio


# 1.1 [2016-01-26]

* ziptool(1): command line tool to modify zip archives
* Speedups for archives with many entries
* Coverity fixes
* Better APK support
* Support for running tests on Windows
* More build fixes for Windows
* Portability fixes
* Documentation improvements


# 1.0.1 [2015-05-04]

* Build fixes for Windows


# 1.0 [2015-05-03]

* Implemented an I/O abstraction layer
* Added support for native Windows API for files
* Added support for setting the last modification time for a file
* Added a new type zip_error_t for errors
* Added more typedefs for structs
* Torrentzip support was removed
* CVE-2015-2331 was fixed
* Addressed all Coverity CIDs


# 0.11.2 [2013-12-19]

* Support querying/setting operating system and external attributes
* For newly added files, set operating system to UNIX, permissions
  to 0666 (0777 for directories)
* Fix bug when writing zip archives containing files bigger than 4GB


# 0.11.1 [2013-04-27]

* Fix bugs in zip_set_file_compression()
* Include Xcode build infrastructure


# 0.11 [2013-03-23]

* Added Zip64 support (large file support)
* Added UTF-8 support for file names, file comments, and archive comments
* Changed API for name and comment related functions for UTF-8 support
* Added zip_discard()
* Added ZIP_TRUNCATE for zip_open()
* Added zip_set_file_compression()
* Added API for accessing and modifying extra fields
* Improved API type consistency
* Use gcc4's visibility __attribute__
* More changes for Windows support
* Additional test cases


# 0.10.1 [2012-03-20]

* Fixed CVE-2012-1162
* Fixed CVE-2012-1163


# 0.10 [2010-03-18]

* Added zip_get_num_entries(), deprecated zip_get_num_files()
* Better windows support
* Support for traditional PKWARE encryption added
* Fix opening archives with more than 65535 entries
* Fix some memory leaks
* Fix cmake build and installation
* Fix memory leak in error case in zip_open()
* Fixed CVE-2011-0421 (no security implications though)
* More documentation


# 0.9.3 [2010-02-01]

* Include m4/ directory in distribution; some packagers need it


# 0.9.2 [2010-01-31]

* Avoid passing uninitialized data to deflate()
* Fix memory leak when closing zip archives


# 0.9.1 [2010-01-24]

* Fix infinite loop on reading some broken files
* Optimization in time conversion (don't call localtime())
* Clear data descriptor flag in central directory, fixing Open Office files
* Allow more than 64k entries


# 0.9 [2008-07-25]

* on Windows, explicitly set dllimport/dllexport
* remove erroneous references to GPL
* add support for torrentzip
* new functions: zip_get_archive_flag, zip_set_archive_flag
* zip_source_zip: add flag to force recompression
* zip_sorce_file: only keep file open while reading from it


# 0.8 [2007-06-06]

* fix for zip archives larger than 2GiB
* fix zip_error_strerror to include libzip error string
* add support for reading streamed zip files
* new functions: zip_add_dir, zip_error_clear, zip_file_error_clear
* add basic support for building with CMake (incomplete)


# 0.7.1 [2006-05-18]

* bugfix for zip_close


# 0.7 [2006-05-06]

* struct zip_stat increased for future encryption support
* zip_add return value changed (now returns new index of added file)
* shared library major bump because of previous two
* added functions for reading and writing file and archive comments
  New functions: zip_get_archive_comment, zip_get_file_comment,
  zip_set_archive_comment, zip_set_file_comment, zip_unchange_archive


# 0.6.1 [2005-07-14]

* various bug fixes


# 0.6 [2005-06-09]

* first standalone release
* changed license to three-clause BSD
* overhauled API
* added man pages
* install zipcmp and zipmerge
