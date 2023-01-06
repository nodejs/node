#ifndef _HAD_ZIP_SOURCE_FILE_WIN32_H
#define _HAD_ZIP_SOURCE_FILE_WIN32_H

/*
  zip_source_file_win32.h -- common header for Windows file implementation
  Copyright (C) 2020 Dieter Baron and Thomas Klausner

  This file is part of libzip, a library to manipulate ZIP archives.
  The authors can be contacted at <info@libzip.org>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
  3. The names of the authors may not be used to endorse or promote
     products derived from this software without specific prior
     written permission.

  THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
  IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* 0x0501 => Windows XP; needs to be at least this value because of GetFileSizeEx */
#if !defined(MS_UWP) && !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0501
#endif

#include <windows.h>

#include <aclapi.h>

#include <stdlib.h>

#include "zipint.h"

#include "zip_source_file.h"

struct zip_win32_file_operations {
    char *(*allocate_tempname)(const char *name, size_t extra_chars, size_t *lengthp);
    HANDLE(__stdcall *create_file)(const void *name, DWORD access, DWORD share_mode, PSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition, DWORD file_attributes, HANDLE template_file);
    BOOL(__stdcall *delete_file)(const void *name);
    DWORD(__stdcall *get_file_attributes)(const void *name);
    BOOL(__stdcall *get_file_attributes_ex)(const void *name, GET_FILEEX_INFO_LEVELS info_level, void *information);
    void (*make_tempname)(char *buf, size_t len, const char *name, zip_uint32_t i);
    BOOL(__stdcall *move_file)(const void *from, const void *to, DWORD flags);
    BOOL(__stdcall *set_file_attributes)(const void *name, DWORD attributes);
    char *(*string_duplicate)(const char *string);
};

typedef struct zip_win32_file_operations zip_win32_file_operations_t;

extern zip_source_file_operations_t _zip_source_file_win32_named_ops;

void _zip_win32_op_close(zip_source_file_context_t *ctx);
zip_int64_t _zip_win32_op_read(zip_source_file_context_t *ctx, void *buf, zip_uint64_t len);
bool _zip_win32_op_seek(zip_source_file_context_t *ctx, void *f, zip_int64_t offset, int whence);
zip_int64_t _zip_win32_op_tell(zip_source_file_context_t *ctx, void *f);

bool _zip_filetime_to_time_t(FILETIME ft, time_t *t);
int _zip_win32_error_to_errno(DWORD win32err);

#ifdef __clang__
#define DONT_WARN_INCOMPATIBLE_FN_PTR_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wincompatible-function-pointer-types\"")
#define DONT_WARN_INCOMPATIBLE_FN_PTR_END _Pragma("GCC diagnostic pop")
#else
#define DONT_WARN_INCOMPATIBLE_FN_PTR_BEGIN
#define DONT_WARN_INCOMPATIBLE_FN_PTR_END
#endif

#endif /* _HAD_ZIP_SOURCE_FILE_WIN32_H */
