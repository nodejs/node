/*
  This file was generated automatically by CMake
  from zip.h and zipint.h; make changes there.
*/

#include "zipint.h"

#define L ZIP_ET_LIBZIP
#define N ZIP_ET_NONE
#define S ZIP_ET_SYS
#define Z ZIP_ET_ZLIB

#define E ZIP_DETAIL_ET_ENTRY
#define G ZIP_DETAIL_ET_GLOBAL

const struct _zip_err_info _zip_err_str[] = {
    { N, "No error" },
    { N, "Multi-disk zip archives not supported" },
    { S, "Renaming temporary file failed" },
    { S, "Closing zip archive failed" },
    { S, "Seek error" },
    { S, "Read error" },
    { S, "Write error" },
    { N, "CRC error" },
    { N, "Containing zip archive was closed" },
    { N, "No such file" },
    { N, "File already exists" },
    { S, "Can't open file" },
    { S, "Failure to create temporary file" },
    { Z, "Zlib error" },
    { N, "Malloc failure" },
    { N, "Entry has been changed" },
    { N, "Compression method not supported" },
    { N, "Premature end of file" },
    { N, "Invalid argument" },
    { N, "Not a zip archive" },
    { N, "Internal error" },
    { L, "Zip archive inconsistent" },
    { S, "Can't remove file" },
    { N, "Entry has been deleted" },
    { N, "Encryption method not supported" },
    { N, "Read-only archive" },
    { N, "No password provided" },
    { N, "Wrong password provided" },
    { N, "Operation not supported" },
    { N, "Resource still in use" },
    { S, "Tell error" },
    { N, "Compressed data invalid" },
    { N, "Operation cancelled" },
    { N, "Unexpected length of data" },
};

const int _zip_err_str_count = sizeof(_zip_err_str)/sizeof(_zip_err_str[0]);

const struct _zip_err_info _zip_err_details[] = {
    { G, "no detail" },
    { G, "central directory overlaps EOCD, or there is space between them" },
    { G, "archive comment length incorrect" },
    { G, "central directory length invalid" },
    { E, "central header invalid" },
    { G, "central directory count of entries is incorrect" },
    { E, "local and central headers do not match" },
    { G, "wrong EOCD length" },
    { G, "EOCD64 overlaps EOCD, or there is space between them" },
    { G, "EOCD64 magic incorrect" },
    { G, "EOCD64 and EOCD do not match" },
    { G, "invalid value in central directory" },
    { E, "variable size fields overflow header" },
    { E, "invalid UTF-8 in filename" },
    { E, "invalid UTF-8 in comment" },
    { E, "invalid Zip64 extra field" },
    { E, "invalid WinZip AES extra field" },
    { E, "garbage at end of extra fields" },
    { E, "extra field length is invalid" },
    { E, "file length in header doesn't match actual file length" },
};

const int _zip_err_details_count = sizeof(_zip_err_details)/sizeof(_zip_err_details[0]);
