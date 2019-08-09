/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <assert.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <wchar.h>
#include <malloc.h>    /* alloca */

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "req-inl.h"


#define SIGKILL         9


typedef struct env_var {
  const WCHAR* const wide;
  const WCHAR* const wide_eq;
  const size_t len; /* including null or '=' */
} env_var_t;

#define E_V(str) { L##str, L##str L"=", sizeof(str) }

static const env_var_t required_vars[] = { /* keep me sorted */
  E_V("HOMEDRIVE"),
  E_V("HOMEPATH"),
  E_V("LOGONSERVER"),
  E_V("PATH"),
  E_V("SYSTEMDRIVE"),
  E_V("SYSTEMROOT"),
  E_V("TEMP"),
  E_V("USERDOMAIN"),
  E_V("USERNAME"),
  E_V("USERPROFILE"),
  E_V("WINDIR"),
};
static size_t n_required_vars = ARRAY_SIZE(required_vars);


static HANDLE uv_global_job_handle_;
static uv_once_t uv_global_job_handle_init_guard_ = UV_ONCE_INIT;


static void uv__init_global_job_handle(void) {
  /* Create a job object and set it up to kill all contained processes when
   * it's closed. Since this handle is made non-inheritable and we're not
   * giving it to anyone, we're the only process holding a reference to it.
   * That means that if this process exits it is closed and all the processes
   * it contains are killed. All processes created with uv_spawn that are not
   * spawned with the UV_PROCESS_DETACHED flag are assigned to this job.
   *
   * We're setting the JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK flag so only the
   * processes that we explicitly add are affected, and *their* subprocesses
   * are not. This ensures that our child processes are not limited in their
   * ability to use job control on Windows versions that don't deal with
   * nested jobs (prior to Windows 8 / Server 2012). It also lets our child
   * processes created detached processes without explicitly breaking away
   * from job control (which uv_spawn doesn't, either).
   */
  SECURITY_ATTRIBUTES attr;
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;

  memset(&attr, 0, sizeof attr);
  attr.bInheritHandle = FALSE;

  memset(&info, 0, sizeof info);
  info.BasicLimitInformation.LimitFlags =
      JOB_OBJECT_LIMIT_BREAKAWAY_OK |
      JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK |
      JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

  uv_global_job_handle_ = CreateJobObjectW(&attr, NULL);
  if (uv_global_job_handle_ == NULL)
    uv_fatal_error(GetLastError(), "CreateJobObjectW");

  if (!SetInformationJobObject(uv_global_job_handle_,
                               JobObjectExtendedLimitInformation,
                               &info,
                               sizeof info))
    uv_fatal_error(GetLastError(), "SetInformationJobObject");
}


static int uv_utf8_to_utf16_alloc(const char* s, WCHAR** ws_ptr) {
  int ws_len, r;
  WCHAR* ws;

  ws_len = MultiByteToWideChar(CP_UTF8,
                               0,
                               s,
                               -1,
                               NULL,
                               0);
  if (ws_len <= 0) {
    return GetLastError();
  }

  ws = (WCHAR*) uv__malloc(ws_len * sizeof(WCHAR));
  if (ws == NULL) {
    return ERROR_OUTOFMEMORY;
  }

  r = MultiByteToWideChar(CP_UTF8,
                          0,
                          s,
                          -1,
                          ws,
                          ws_len);
  assert(r == ws_len);

  *ws_ptr = ws;
  return 0;
}


static void uv_process_init(uv_loop_t* loop, uv_process_t* handle) {
  uv__handle_init(loop, (uv_handle_t*) handle, UV_PROCESS);
  handle->exit_cb = NULL;
  handle->pid = 0;
  handle->exit_signal = 0;
  handle->wait_handle = INVALID_HANDLE_VALUE;
  handle->process_handle = INVALID_HANDLE_VALUE;
  handle->child_stdio_buffer = NULL;
  handle->exit_cb_pending = 0;

  UV_REQ_INIT(&handle->exit_req, UV_PROCESS_EXIT);
  handle->exit_req.data = handle;
}


/*
 * Path search functions
 */

/*
 * Helper function for search_path
 */
static WCHAR* search_path_join_test(const WCHAR* dir,
                                    size_t dir_len,
                                    const WCHAR* name,
                                    size_t name_len,
                                    const WCHAR* ext,
                                    size_t ext_len,
                                    const WCHAR* cwd,
                                    size_t cwd_len) {
  WCHAR *result, *result_pos;
  DWORD attrs;
  if (dir_len > 2 && dir[0] == L'\\' && dir[1] == L'\\') {
    /* It's a UNC path so ignore cwd */
    cwd_len = 0;
  } else if (dir_len >= 1 && (dir[0] == L'/' || dir[0] == L'\\')) {
    /* It's a full path without drive letter, use cwd's drive letter only */
    cwd_len = 2;
  } else if (dir_len >= 2 && dir[1] == L':' &&
      (dir_len < 3 || (dir[2] != L'/' && dir[2] != L'\\'))) {
    /* It's a relative path with drive letter (ext.g. D:../some/file)
     * Replace drive letter in dir by full cwd if it points to the same drive,
     * otherwise use the dir only.
     */
    if (cwd_len < 2 || _wcsnicmp(cwd, dir, 2) != 0) {
      cwd_len = 0;
    } else {
      dir += 2;
      dir_len -= 2;
    }
  } else if (dir_len > 2 && dir[1] == L':') {
    /* It's an absolute path with drive letter
     * Don't use the cwd at all
     */
    cwd_len = 0;
  }

  /* Allocate buffer for output */
  result = result_pos = (WCHAR*)uv__malloc(sizeof(WCHAR) *
      (cwd_len + 1 + dir_len + 1 + name_len + 1 + ext_len + 1));

  /* Copy cwd */
  wcsncpy(result_pos, cwd, cwd_len);
  result_pos += cwd_len;

  /* Add a path separator if cwd didn't end with one */
  if (cwd_len && wcsrchr(L"\\/:", result_pos[-1]) == NULL) {
    result_pos[0] = L'\\';
    result_pos++;
  }

  /* Copy dir */
  wcsncpy(result_pos, dir, dir_len);
  result_pos += dir_len;

  /* Add a separator if the dir didn't end with one */
  if (dir_len && wcsrchr(L"\\/:", result_pos[-1]) == NULL) {
    result_pos[0] = L'\\';
    result_pos++;
  }

  /* Copy filename */
  wcsncpy(result_pos, name, name_len);
  result_pos += name_len;

  if (ext_len) {
    /* Add a dot if the filename didn't end with one */
    if (name_len && result_pos[-1] != '.') {
      result_pos[0] = L'.';
      result_pos++;
    }

    /* Copy extension */
    wcsncpy(result_pos, ext, ext_len);
    result_pos += ext_len;
  }

  /* Null terminator */
  result_pos[0] = L'\0';

  attrs = GetFileAttributesW(result);

  if (attrs != INVALID_FILE_ATTRIBUTES &&
      !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
    return result;
  }

  uv__free(result);
  return NULL;
}


/*
 * Helper function for search_path
 */
static WCHAR* path_search_walk_ext(const WCHAR *dir,
                                   size_t dir_len,
                                   const WCHAR *name,
                                   size_t name_len,
                                   WCHAR *cwd,
                                   size_t cwd_len,
                                   int name_has_ext) {
  WCHAR* result;

  /* If the name itself has a nonempty extension, try this extension first */
  if (name_has_ext) {
    result = search_path_join_test(dir, dir_len,
                                   name, name_len,
                                   L"", 0,
                                   cwd, cwd_len);
    if (result != NULL) {
      return result;
    }
  }

  /* Try .com extension */
  result = search_path_join_test(dir, dir_len,
                                 name, name_len,
                                 L"com", 3,
                                 cwd, cwd_len);
  if (result != NULL) {
    return result;
  }

  /* Try .exe extension */
  result = search_path_join_test(dir, dir_len,
                                 name, name_len,
                                 L"exe", 3,
                                 cwd, cwd_len);
  if (result != NULL) {
    return result;
  }

  return NULL;
}


/*
 * search_path searches the system path for an executable filename -
 * the windows API doesn't provide this as a standalone function nor as an
 * option to CreateProcess.
 *
 * It tries to return an absolute filename.
 *
 * Furthermore, it tries to follow the semantics that cmd.exe, with this
 * exception that PATHEXT environment variable isn't used. Since CreateProcess
 * can start only .com and .exe files, only those extensions are tried. This
 * behavior equals that of msvcrt's spawn functions.
 *
 * - Do not search the path if the filename already contains a path (either
 *   relative or absolute).
 *
 * - If there's really only a filename, check the current directory for file,
 *   then search all path directories.
 *
 * - If filename specified has *any* extension, search for the file with the
 *   specified extension first.
 *
 * - If the literal filename is not found in a directory, try *appending*
 *   (not replacing) .com first and then .exe.
 *
 * - The path variable may contain relative paths; relative paths are relative
 *   to the cwd.
 *
 * - Directories in path may or may not end with a trailing backslash.
 *
 * - CMD does not trim leading/trailing whitespace from path/pathex entries
 *   nor from the environment variables as a whole.
 *
 * - When cmd.exe cannot read a directory, it will just skip it and go on
 *   searching. However, unlike posix-y systems, it will happily try to run a
 *   file that is not readable/executable; if the spawn fails it will not
 *   continue searching.
 *
 * UNC path support: we are dealing with UNC paths in both the path and the
 * filename. This is a deviation from what cmd.exe does (it does not let you
 * start a program by specifying an UNC path on the command line) but this is
 * really a pointless restriction.
 *
 */
static WCHAR* search_path(const WCHAR *file,
                            WCHAR *cwd,
                            const WCHAR *path) {
  int file_has_dir;
  WCHAR* result = NULL;
  WCHAR *file_name_start;
  WCHAR *dot;
  const WCHAR *dir_start, *dir_end, *dir_path;
  size_t dir_len;
  int name_has_ext;

  size_t file_len = wcslen(file);
  size_t cwd_len = wcslen(cwd);

  /* If the caller supplies an empty filename,
   * we're not gonna return c:\windows\.exe -- GFY!
   */
  if (file_len == 0
      || (file_len == 1 && file[0] == L'.')) {
    return NULL;
  }

  /* Find the start of the filename so we can split the directory from the
   * name. */
  for (file_name_start = (WCHAR*)file + file_len;
       file_name_start > file
           && file_name_start[-1] != L'\\'
           && file_name_start[-1] != L'/'
           && file_name_start[-1] != L':';
       file_name_start--);

  file_has_dir = file_name_start != file;

  /* Check if the filename includes an extension */
  dot = wcschr(file_name_start, L'.');
  name_has_ext = (dot != NULL && dot[1] != L'\0');

  if (file_has_dir) {
    /* The file has a path inside, don't use path */
    result = path_search_walk_ext(
        file, file_name_start - file,
        file_name_start, file_len - (file_name_start - file),
        cwd, cwd_len,
        name_has_ext);

  } else {
    dir_end = path;

    /* The file is really only a name; look in cwd first, then scan path */
    result = path_search_walk_ext(L"", 0,
                                  file, file_len,
                                  cwd, cwd_len,
                                  name_has_ext);

    while (result == NULL) {
      if (*dir_end == L'\0') {
        break;
      }

      /* Skip the separator that dir_end now points to */
      if (dir_end != path || *path == L';') {
        dir_end++;
      }

      /* Next slice starts just after where the previous one ended */
      dir_start = dir_end;

      /* If path is quoted, find quote end */
      if (*dir_start == L'"' || *dir_start == L'\'') {
        dir_end = wcschr(dir_start + 1, *dir_start);
        if (dir_end == NULL) {
          dir_end = wcschr(dir_start, L'\0');
        }
      }
      /* Slice until the next ; or \0 is found */
      dir_end = wcschr(dir_end, L';');
      if (dir_end == NULL) {
        dir_end = wcschr(dir_start, L'\0');
      }

      /* If the slice is zero-length, don't bother */
      if (dir_end - dir_start == 0) {
        continue;
      }

      dir_path = dir_start;
      dir_len = dir_end - dir_start;

      /* Adjust if the path is quoted. */
      if (dir_path[0] == '"' || dir_path[0] == '\'') {
        ++dir_path;
        --dir_len;
      }

      if (dir_path[dir_len - 1] == '"' || dir_path[dir_len - 1] == '\'') {
        --dir_len;
      }

      result = path_search_walk_ext(dir_path, dir_len,
                                    file, file_len,
                                    cwd, cwd_len,
                                    name_has_ext);
    }
  }

  return result;
}


/*
 * Quotes command line arguments
 * Returns a pointer to the end (next char to be written) of the buffer
 */
WCHAR* quote_cmd_arg(const WCHAR *source, WCHAR *target) {
  size_t len = wcslen(source);
  size_t i;
  int quote_hit;
  WCHAR* start;

  if (len == 0) {
    /* Need double quotation for empty argument */
    *(target++) = L'"';
    *(target++) = L'"';
    return target;
  }

  if (NULL == wcspbrk(source, L" \t\"")) {
    /* No quotation needed */
    wcsncpy(target, source, len);
    target += len;
    return target;
  }

  if (NULL == wcspbrk(source, L"\"\\")) {
    /*
     * No embedded double quotes or backlashes, so I can just wrap
     * quote marks around the whole thing.
     */
    *(target++) = L'"';
    wcsncpy(target, source, len);
    target += len;
    *(target++) = L'"';
    return target;
  }

  /*
   * Expected input/output:
   *   input : hello"world
   *   output: "hello\"world"
   *   input : hello""world
   *   output: "hello\"\"world"
   *   input : hello\world
   *   output: hello\world
   *   input : hello\\world
   *   output: hello\\world
   *   input : hello\"world
   *   output: "hello\\\"world"
   *   input : hello\\"world
   *   output: "hello\\\\\"world"
   *   input : hello world\
   *   output: "hello world\\"
   */

  *(target++) = L'"';
  start = target;
  quote_hit = 1;

  for (i = len; i > 0; --i) {
    *(target++) = source[i - 1];

    if (quote_hit && source[i - 1] == L'\\') {
      *(target++) = L'\\';
    } else if(source[i - 1] == L'"') {
      quote_hit = 1;
      *(target++) = L'\\';
    } else {
      quote_hit = 0;
    }
  }
  target[0] = L'\0';
  wcsrev(start);
  *(target++) = L'"';
  return target;
}


int make_program_args(char** args, int verbatim_arguments, WCHAR** dst_ptr) {
  char** arg;
  WCHAR* dst = NULL;
  WCHAR* temp_buffer = NULL;
  size_t dst_len = 0;
  size_t temp_buffer_len = 0;
  WCHAR* pos;
  int arg_count = 0;
  int err = 0;

  /* Count the required size. */
  for (arg = args; *arg; arg++) {
    DWORD arg_len;

    arg_len = MultiByteToWideChar(CP_UTF8,
                                  0,
                                  *arg,
                                  -1,
                                  NULL,
                                  0);
    if (arg_len == 0) {
      return GetLastError();
    }

    dst_len += arg_len;

    if (arg_len > temp_buffer_len)
      temp_buffer_len = arg_len;

    arg_count++;
  }

  /* Adjust for potential quotes. Also assume the worst-case scenario that
   * every character needs escaping, so we need twice as much space. */
  dst_len = dst_len * 2 + arg_count * 2;

  /* Allocate buffer for the final command line. */
  dst = (WCHAR*) uv__malloc(dst_len * sizeof(WCHAR));
  if (dst == NULL) {
    err = ERROR_OUTOFMEMORY;
    goto error;
  }

  /* Allocate temporary working buffer. */
  temp_buffer = (WCHAR*) uv__malloc(temp_buffer_len * sizeof(WCHAR));
  if (temp_buffer == NULL) {
    err = ERROR_OUTOFMEMORY;
    goto error;
  }

  pos = dst;
  for (arg = args; *arg; arg++) {
    DWORD arg_len;

    /* Convert argument to wide char. */
    arg_len = MultiByteToWideChar(CP_UTF8,
                                  0,
                                  *arg,
                                  -1,
                                  temp_buffer,
                                  (int) (dst + dst_len - pos));
    if (arg_len == 0) {
      err = GetLastError();
      goto error;
    }

    if (verbatim_arguments) {
      /* Copy verbatim. */
      wcscpy(pos, temp_buffer);
      pos += arg_len - 1;
    } else {
      /* Quote/escape, if needed. */
      pos = quote_cmd_arg(temp_buffer, pos);
    }

    *pos++ = *(arg + 1) ? L' ' : L'\0';
  }

  uv__free(temp_buffer);

  *dst_ptr = dst;
  return 0;

error:
  uv__free(dst);
  uv__free(temp_buffer);
  return err;
}


int env_strncmp(const wchar_t* a, int na, const wchar_t* b) {
  wchar_t* a_eq;
  wchar_t* b_eq;
  wchar_t* A;
  wchar_t* B;
  int nb;
  int r;

  if (na < 0) {
    a_eq = wcschr(a, L'=');
    assert(a_eq);
    na = (int)(long)(a_eq - a);
  } else {
    na--;
  }
  b_eq = wcschr(b, L'=');
  assert(b_eq);
  nb = b_eq - b;

  A = alloca((na+1) * sizeof(wchar_t));
  B = alloca((nb+1) * sizeof(wchar_t));

  r = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, a, na, A, na);
  assert(r==na);
  A[na] = L'\0';
  r = LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, b, nb, B, nb);
  assert(r==nb);
  B[nb] = L'\0';

  while (1) {
    wchar_t AA = *A++;
    wchar_t BB = *B++;
    if (AA < BB) {
      return -1;
    } else if (AA > BB) {
      return 1;
    } else if (!AA && !BB) {
      return 0;
    }
  }
}


static int qsort_wcscmp(const void *a, const void *b) {
  wchar_t* astr = *(wchar_t* const*)a;
  wchar_t* bstr = *(wchar_t* const*)b;
  return env_strncmp(astr, -1, bstr);
}


/*
 * The way windows takes environment variables is different than what C does;
 * Windows wants a contiguous block of null-terminated strings, terminated
 * with an additional null.
 *
 * Windows has a few "essential" environment variables. winsock will fail
 * to initialize if SYSTEMROOT is not defined; some APIs make reference to
 * TEMP. SYSTEMDRIVE is probably also important. We therefore ensure that
 * these get defined if the input environment block does not contain any
 * values for them.
 *
 * Also add variables known to Cygwin to be required for correct
 * subprocess operation in many cases:
 * https://github.com/Alexpux/Cygwin/blob/b266b04fbbd3a595f02ea149e4306d3ab9b1fe3d/winsup/cygwin/environ.cc#L955
 *
 */
int make_program_env(char* env_block[], WCHAR** dst_ptr) {
  WCHAR* dst;
  WCHAR* ptr;
  char** env;
  size_t env_len = 0;
  int len;
  size_t i;
  DWORD var_size;
  size_t env_block_count = 1; /* 1 for null-terminator */
  WCHAR* dst_copy;
  WCHAR** ptr_copy;
  WCHAR** env_copy;
  DWORD* required_vars_value_len = alloca(n_required_vars * sizeof(DWORD*));

  /* first pass: determine size in UTF-16 */
  for (env = env_block; *env; env++) {
    int len;
    if (strchr(*env, '=')) {
      len = MultiByteToWideChar(CP_UTF8,
                                0,
                                *env,
                                -1,
                                NULL,
                                0);
      if (len <= 0) {
        return GetLastError();
      }
      env_len += len;
      env_block_count++;
    }
  }

  /* second pass: copy to UTF-16 environment block */
  dst_copy = (WCHAR*)uv__malloc(env_len * sizeof(WCHAR));
  if (dst_copy == NULL && env_len > 0) {
    return ERROR_OUTOFMEMORY;
  }
  env_copy = alloca(env_block_count * sizeof(WCHAR*));

  ptr = dst_copy;
  ptr_copy = env_copy;
  for (env = env_block; *env; env++) {
    if (strchr(*env, '=')) {
      len = MultiByteToWideChar(CP_UTF8,
                                0,
                                *env,
                                -1,
                                ptr,
                                (int) (env_len - (ptr - dst_copy)));
      if (len <= 0) {
        DWORD err = GetLastError();
        uv__free(dst_copy);
        return err;
      }
      *ptr_copy++ = ptr;
      ptr += len;
    }
  }
  *ptr_copy = NULL;
  assert(env_len == 0 || env_len == (size_t) (ptr - dst_copy));

  /* sort our (UTF-16) copy */
  qsort(env_copy, env_block_count-1, sizeof(wchar_t*), qsort_wcscmp);

  /* third pass: check for required variables */
  for (ptr_copy = env_copy, i = 0; i < n_required_vars; ) {
    int cmp;
    if (!*ptr_copy) {
      cmp = -1;
    } else {
      cmp = env_strncmp(required_vars[i].wide_eq,
                       required_vars[i].len,
                        *ptr_copy);
    }
    if (cmp < 0) {
      /* missing required var */
      var_size = GetEnvironmentVariableW(required_vars[i].wide, NULL, 0);
      required_vars_value_len[i] = var_size;
      if (var_size != 0) {
        env_len += required_vars[i].len;
        env_len += var_size;
      }
      i++;
    } else {
      ptr_copy++;
      if (cmp == 0)
        i++;
    }
  }

  /* final pass: copy, in sort order, and inserting required variables */
  dst = uv__malloc((1+env_len) * sizeof(WCHAR));
  if (!dst) {
    uv__free(dst_copy);
    return ERROR_OUTOFMEMORY;
  }

  for (ptr = dst, ptr_copy = env_copy, i = 0;
       *ptr_copy || i < n_required_vars;
       ptr += len) {
    int cmp;
    if (i >= n_required_vars) {
      cmp = 1;
    } else if (!*ptr_copy) {
      cmp = -1;
    } else {
      cmp = env_strncmp(required_vars[i].wide_eq,
                        required_vars[i].len,
                        *ptr_copy);
    }
    if (cmp < 0) {
      /* missing required var */
      len = required_vars_value_len[i];
      if (len) {
        wcscpy(ptr, required_vars[i].wide_eq);
        ptr += required_vars[i].len;
        var_size = GetEnvironmentVariableW(required_vars[i].wide,
                                           ptr,
                                           (int) (env_len - (ptr - dst)));
        if (var_size != (DWORD) (len - 1)) { /* TODO: handle race condition? */
          uv_fatal_error(GetLastError(), "GetEnvironmentVariableW");
        }
      }
      i++;
    } else {
      /* copy var from env_block */
      len = wcslen(*ptr_copy) + 1;
      wmemcpy(ptr, *ptr_copy, len);
      ptr_copy++;
      if (cmp == 0)
        i++;
    }
  }

  /* Terminate with an extra NULL. */
  assert(env_len == (size_t) (ptr - dst));
  *ptr = L'\0';

  uv__free(dst_copy);
  *dst_ptr = dst;
  return 0;
}

/*
 * Attempt to find the value of the PATH environment variable in the child's
 * preprocessed environment.
 *
 * If found, a pointer into `env` is returned. If not found, NULL is returned.
 */
static WCHAR* find_path(WCHAR *env) {
  for (; env != NULL && *env != 0; env += wcslen(env) + 1) {
    if ((env[0] == L'P' || env[0] == L'p') &&
        (env[1] == L'A' || env[1] == L'a') &&
        (env[2] == L'T' || env[2] == L't') &&
        (env[3] == L'H' || env[3] == L'h') &&
        (env[4] == L'=')) {
      return &env[5];
    }
  }

  return NULL;
}

/*
 * Called on Windows thread-pool thread to indicate that
 * a child process has exited.
 */
static void CALLBACK exit_wait_callback(void* data, BOOLEAN didTimeout) {
  uv_process_t* process = (uv_process_t*) data;
  uv_loop_t* loop = process->loop;

  assert(didTimeout == FALSE);
  assert(process);
  assert(!process->exit_cb_pending);

  process->exit_cb_pending = 1;

  /* Post completed */
  POST_COMPLETION_FOR_REQ(loop, &process->exit_req);
}


/* Called on main thread after a child process has exited. */
void uv_process_proc_exit(uv_loop_t* loop, uv_process_t* handle) {
  int64_t exit_code;
  DWORD status;

  assert(handle->exit_cb_pending);
  handle->exit_cb_pending = 0;

  /* If we're closing, don't call the exit callback. Just schedule a close
   * callback now. */
  if (handle->flags & UV_HANDLE_CLOSING) {
    uv_want_endgame(loop, (uv_handle_t*) handle);
    return;
  }

  /* Unregister from process notification. */
  if (handle->wait_handle != INVALID_HANDLE_VALUE) {
    UnregisterWait(handle->wait_handle);
    handle->wait_handle = INVALID_HANDLE_VALUE;
  }

  /* Set the handle to inactive: no callbacks will be made after the exit
   * callback. */
  uv__handle_stop(handle);

  if (GetExitCodeProcess(handle->process_handle, &status)) {
    exit_code = status;
  } else {
    /* Unable to obtain the exit code. This should never happen. */
    exit_code = uv_translate_sys_error(GetLastError());
  }

  /* Fire the exit callback. */
  if (handle->exit_cb) {
    handle->exit_cb(handle, exit_code, handle->exit_signal);
  }
}


void uv_process_close(uv_loop_t* loop, uv_process_t* handle) {
  uv__handle_closing(handle);

  if (handle->wait_handle != INVALID_HANDLE_VALUE) {
    /* This blocks until either the wait was cancelled, or the callback has
     * completed. */
    BOOL r = UnregisterWaitEx(handle->wait_handle, INVALID_HANDLE_VALUE);
    if (!r) {
      /* This should never happen, and if it happens, we can't recover... */
      uv_fatal_error(GetLastError(), "UnregisterWaitEx");
    }

    handle->wait_handle = INVALID_HANDLE_VALUE;
  }

  if (!handle->exit_cb_pending) {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }
}


void uv_process_endgame(uv_loop_t* loop, uv_process_t* handle) {
  assert(!handle->exit_cb_pending);
  assert(handle->flags & UV_HANDLE_CLOSING);
  assert(!(handle->flags & UV_HANDLE_CLOSED));

  /* Clean-up the process handle. */
  CloseHandle(handle->process_handle);

  uv__handle_close(handle);
}


int uv_spawn(uv_loop_t* loop,
             uv_process_t* process,
             const uv_process_options_t* options) {
  int i;
  int err = 0;
  WCHAR* path = NULL, *alloc_path = NULL;
  BOOL result;
  WCHAR* application_path = NULL, *application = NULL, *arguments = NULL,
         *env = NULL, *cwd = NULL;
  STARTUPINFOW startup;
  PROCESS_INFORMATION info;
  DWORD process_flags;

  uv_process_init(loop, process);
  process->exit_cb = options->exit_cb;

  if (options->flags & (UV_PROCESS_SETGID | UV_PROCESS_SETUID)) {
    return UV_ENOTSUP;
  }

  if (options->file == NULL ||
      options->args == NULL) {
    return UV_EINVAL;
  }

  assert(options->file != NULL);
  assert(!(options->flags & ~(UV_PROCESS_DETACHED |
                              UV_PROCESS_SETGID |
                              UV_PROCESS_SETUID |
                              UV_PROCESS_WINDOWS_HIDE |
                              UV_PROCESS_WINDOWS_HIDE_CONSOLE |
                              UV_PROCESS_WINDOWS_HIDE_GUI |
                              UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS)));

  err = uv_utf8_to_utf16_alloc(options->file, &application);
  if (err)
    goto done;

  err = make_program_args(
      options->args,
      options->flags & UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS,
      &arguments);
  if (err)
    goto done;

  if (options->env) {
     err = make_program_env(options->env, &env);
     if (err)
       goto done;
  }

  if (options->cwd) {
    /* Explicit cwd */
    err = uv_utf8_to_utf16_alloc(options->cwd, &cwd);
    if (err)
      goto done;

  } else {
    /* Inherit cwd */
    DWORD cwd_len, r;

    cwd_len = GetCurrentDirectoryW(0, NULL);
    if (!cwd_len) {
      err = GetLastError();
      goto done;
    }

    cwd = (WCHAR*) uv__malloc(cwd_len * sizeof(WCHAR));
    if (cwd == NULL) {
      err = ERROR_OUTOFMEMORY;
      goto done;
    }

    r = GetCurrentDirectoryW(cwd_len, cwd);
    if (r == 0 || r >= cwd_len) {
      err = GetLastError();
      goto done;
    }
  }

  /* Get PATH environment variable. */
  path = find_path(env);
  if (path == NULL) {
    DWORD path_len, r;

    path_len = GetEnvironmentVariableW(L"PATH", NULL, 0);
    if (path_len == 0) {
      err = GetLastError();
      goto done;
    }

    alloc_path = (WCHAR*) uv__malloc(path_len * sizeof(WCHAR));
    if (alloc_path == NULL) {
      err = ERROR_OUTOFMEMORY;
      goto done;
    }
    path = alloc_path;

    r = GetEnvironmentVariableW(L"PATH", path, path_len);
    if (r == 0 || r >= path_len) {
      err = GetLastError();
      goto done;
    }
  }

  err = uv__stdio_create(loop, options, &process->child_stdio_buffer);
  if (err)
    goto done;

  application_path = search_path(application,
                                 cwd,
                                 path);
  if (application_path == NULL) {
    /* Not found. */
    err = ERROR_FILE_NOT_FOUND;
    goto done;
  }

  startup.cb = sizeof(startup);
  startup.lpReserved = NULL;
  startup.lpDesktop = NULL;
  startup.lpTitle = NULL;
  startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

  startup.cbReserved2 = uv__stdio_size(process->child_stdio_buffer);
  startup.lpReserved2 = (BYTE*) process->child_stdio_buffer;

  startup.hStdInput = uv__stdio_handle(process->child_stdio_buffer, 0);
  startup.hStdOutput = uv__stdio_handle(process->child_stdio_buffer, 1);
  startup.hStdError = uv__stdio_handle(process->child_stdio_buffer, 2);

  process_flags = CREATE_UNICODE_ENVIRONMENT;

  if ((options->flags & UV_PROCESS_WINDOWS_HIDE_CONSOLE) ||
      (options->flags & UV_PROCESS_WINDOWS_HIDE)) {
    /* Avoid creating console window if stdio is not inherited. */
    for (i = 0; i < options->stdio_count; i++) {
      if (options->stdio[i].flags & UV_INHERIT_FD)
        break;
      if (i == options->stdio_count - 1)
        process_flags |= CREATE_NO_WINDOW;
    }
  }
  if ((options->flags & UV_PROCESS_WINDOWS_HIDE_GUI) ||
      (options->flags & UV_PROCESS_WINDOWS_HIDE)) {
    /* Use SW_HIDE to avoid any potential process window. */
    startup.wShowWindow = SW_HIDE;
  } else {
    startup.wShowWindow = SW_SHOWDEFAULT;
  }

  if (options->flags & UV_PROCESS_DETACHED) {
    /* Note that we're not setting the CREATE_BREAKAWAY_FROM_JOB flag. That
     * means that libuv might not let you create a fully daemonized process
     * when run under job control. However the type of job control that libuv
     * itself creates doesn't trickle down to subprocesses so they can still
     * daemonize.
     *
     * A reason to not do this is that CREATE_BREAKAWAY_FROM_JOB makes the
     * CreateProcess call fail if we're under job control that doesn't allow
     * breakaway.
     */
    process_flags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
  }

  if (!CreateProcessW(application_path,
                     arguments,
                     NULL,
                     NULL,
                     1,
                     process_flags,
                     env,
                     cwd,
                     &startup,
                     &info)) {
    /* CreateProcessW failed. */
    err = GetLastError();
    goto done;
  }

  /* Spawn succeeded. Beyond this point, failure is reported asynchronously. */

  process->process_handle = info.hProcess;
  process->pid = info.dwProcessId;

  /* If the process isn't spawned as detached, assign to the global job object
   * so windows will kill it when the parent process dies. */
  if (!(options->flags & UV_PROCESS_DETACHED)) {
    uv_once(&uv_global_job_handle_init_guard_, uv__init_global_job_handle);

    if (!AssignProcessToJobObject(uv_global_job_handle_, info.hProcess)) {
      /* AssignProcessToJobObject might fail if this process is under job
       * control and the job doesn't have the
       * JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK flag set, on a Windows version
       * that doesn't support nested jobs.
       *
       * When that happens we just swallow the error and continue without
       * establishing a kill-child-on-parent-exit relationship, otherwise
       * there would be no way for libuv applications run under job control
       * to spawn processes at all.
       */
      DWORD err = GetLastError();
      if (err != ERROR_ACCESS_DENIED)
        uv_fatal_error(err, "AssignProcessToJobObject");
    }
  }

  /* Set IPC pid to all IPC pipes. */
  for (i = 0; i < options->stdio_count; i++) {
    const uv_stdio_container_t* fdopt = &options->stdio[i];
    if (fdopt->flags & UV_CREATE_PIPE &&
        fdopt->data.stream->type == UV_NAMED_PIPE &&
        ((uv_pipe_t*) fdopt->data.stream)->ipc) {
      ((uv_pipe_t*) fdopt->data.stream)->pipe.conn.ipc_remote_pid =
          info.dwProcessId;
    }
  }

  /* Setup notifications for when the child process exits. */
  result = RegisterWaitForSingleObject(&process->wait_handle,
      process->process_handle, exit_wait_callback, (void*)process, INFINITE,
      WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
  if (!result) {
    uv_fatal_error(GetLastError(), "RegisterWaitForSingleObject");
  }

  CloseHandle(info.hThread);

  assert(!err);

  /* Make the handle active. It will remain active until the exit callback is
   * made or the handle is closed, whichever happens first. */
  uv__handle_start(process);

  /* Cleanup, whether we succeeded or failed. */
 done:
  uv__free(application);
  uv__free(application_path);
  uv__free(arguments);
  uv__free(cwd);
  uv__free(env);
  uv__free(alloc_path);

  if (process->child_stdio_buffer != NULL) {
    /* Clean up child stdio handles. */
    uv__stdio_destroy(process->child_stdio_buffer);
    process->child_stdio_buffer = NULL;
  }

  return uv_translate_sys_error(err);
}


static int uv__kill(HANDLE process_handle, int signum) {
  if (signum < 0 || signum >= NSIG) {
    return UV_EINVAL;
  }

  switch (signum) {
    case SIGTERM:
    case SIGKILL:
    case SIGINT: {
      /* Unconditionally terminate the process. On Windows, killed processes
       * normally return 1. */
      DWORD status;
      int err;

      if (TerminateProcess(process_handle, 1))
        return 0;

      /* If the process already exited before TerminateProcess was called,.
       * TerminateProcess will fail with ERROR_ACCESS_DENIED. */
      err = GetLastError();
      if (err == ERROR_ACCESS_DENIED &&
          GetExitCodeProcess(process_handle, &status) &&
          status != STILL_ACTIVE) {
        return UV_ESRCH;
      }

      return uv_translate_sys_error(err);
    }

    case 0: {
      /* Health check: is the process still alive? */
      DWORD status;

      if (!GetExitCodeProcess(process_handle, &status))
        return uv_translate_sys_error(GetLastError());

      if (status != STILL_ACTIVE)
        return UV_ESRCH;

      return 0;
    }

    default:
      /* Unsupported signal. */
      return UV_ENOSYS;
  }
}


int uv_process_kill(uv_process_t* process, int signum) {
  int err;

  if (process->process_handle == INVALID_HANDLE_VALUE) {
    return UV_EINVAL;
  }

  err = uv__kill(process->process_handle, signum);
  if (err) {
    return err;  /* err is already translated. */
  }

  process->exit_signal = signum;

  return 0;
}


int uv_kill(int pid, int signum) {
  int err;
  HANDLE process_handle;

  if (pid == 0) {
    process_handle = GetCurrentProcess();
  } else {
    process_handle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION,
                                 FALSE,
                                 pid);
  }

  if (process_handle == NULL) {
    err = GetLastError();
    if (err == ERROR_INVALID_PARAMETER) {
      return UV_ESRCH;
    } else {
      return uv_translate_sys_error(err);
    }
  }

  err = uv__kill(process_handle, signum);
  CloseHandle(process_handle);

  return err;  /* err is already translated. */
}
