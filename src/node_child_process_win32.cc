// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


#include <node.h>
#include <node_child_process.h>

#include <v8.h>
#include <uv.h>
#include <eio.h>

#include <assert.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include <platform_win32.h>
#include <platform_win32_winsock.h>

namespace node {

using namespace v8;


static const WCHAR DEFAULT_PATH[1] = L"";
static const WCHAR DEFAULT_PATH_EXT[20] = L".COM;.EXE;.BAT;.CMD";


static Persistent<String> pid_symbol;
static Persistent<String> onexit_symbol;


static struct watcher_status_struct {
  uv_async_t async_watcher;
  ChildProcess *child;
  HANDLE lock;
  int num_active;
} watcher_status;


/*
 * Path search functions
 */

/*
 * Helper function for search_path
 */
static inline WCHAR* search_path_join_test(
    const WCHAR* dir, int dir_len, const WCHAR* name, int name_len,
    const WCHAR* ext, int ext_len, const WCHAR* cwd, int cwd_len) {
  WCHAR *result, *result_pos;

  if (dir_len >= 1 && (dir[0] == L'/' || dir[0] == L'\\')) {
    // It's a full path without drive letter, use cwd's drive letter only
    cwd_len = 2;
  } else if (dir_len >= 2 && dir[1] == L':' &&
      (dir_len < 3 || (dir[2] != L'/' && dir[2] != L'\\'))) {
    // It's a relative path with drive letter (ext.g. D:../some/file)
    // Replace drive letter in dir by full cwd if it points to the same drive,
    // otherwise use the dir only.
    if (cwd_len < 2 || _wcsnicmp(cwd, dir, 2) != 0) {
      cwd_len = 0;
    } else {
      dir += 2;
      dir_len -= 2;
    }
  } else if (dir_len > 2 && dir[1] == L':') {
    // It's an absolute path with drive letter
    // Don't use the cwd at all
    cwd_len = 0;
  }

  // Allocate buffer for output
  result = result_pos =
      new WCHAR[cwd_len + 1 + dir_len + 1 + name_len + 1 + ext_len + 1];

  // Copy cwd
  wcsncpy(result_pos, cwd, cwd_len);
  result_pos += cwd_len;

  // Add a path separator if cwd didn't end with one
  if (cwd_len && wcsrchr(L"\\/:", result_pos[-1]) == NULL) {
    result_pos[0] = L'\\';
    result_pos++;
  }

  // Copy dir
  wcsncpy(result_pos, dir, dir_len);
  result_pos += dir_len;

  // Add a separator if the dir didn't end with one
  if (dir_len && wcsrchr(L"\\/:", result_pos[-1]) == NULL) {
    result_pos[0] = L'\\';
    result_pos++;
  }

  // Copy filename
  wcsncpy(result_pos, name, name_len);
  result_pos += name_len;

  // Copy extension
  if (ext_len) {
    result_pos[0] = L'.';
    result_pos++;
    wcsncpy(result_pos, ext, ext_len);
    result_pos += ext_len;
  }

  // Null terminator
  result_pos[0] = L'\0';

  DWORD attrs = GetFileAttributesW(result);

  if (attrs != INVALID_FILE_ATTRIBUTES &&
	 !(attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
    return result;
  }

  delete[] result;
  return NULL;
}


/*
 * Helper function for search_path
 */
static inline WCHAR* path_search_walk_ext(
    const WCHAR *dir, int dir_len, const WCHAR *name, int name_len,
    WCHAR *cwd, int cwd_len, const WCHAR *path_ext, bool name_has_ext) {
  WCHAR* result = NULL;

  const WCHAR *ext_start,
              *ext_end = path_ext;

  // If the name itself has a nonemtpy extension, try this extension first
  if (name_has_ext) {
    result = search_path_join_test(dir, dir_len,
                                   name, name_len,
                                   L"", 0,
                                   cwd, cwd_len);
  }

  // Add path_ext extensions and try to find a name that matches
  while (result == NULL) {
    if (*ext_end == L'\0') {
      break;
    }

    // Skip the separator that ext_end now points to
    if (ext_end != path_ext) {
      ext_end++;
    }

    // Find the next dot in path_ext
    ext_start = wcschr(ext_end, L'.');
    if (ext_start == NULL) {
      break;
    }

    // Skip the dot
    ext_start++;

    // Slice until we found a ; or alternatively a \0
    ext_end = wcschr(ext_start, L';');
    if (ext_end == NULL) {
       ext_end = wcschr(ext_start, '\0');
    }

    result = search_path_join_test(dir, dir_len,
                                   name, name_len,
                                   ext_start, (ext_end - ext_start),
                                   cwd, cwd_len);
  }

  return result;
}


/*
 * search_path searches the system path for an executable filename -
 * the windows API doesn't provide this as a standalone function nor as an
 * option to CreateProcess.
 *
 * It tries to return an absolute filename.
 *
 * Furthermore, it tries to follow the semantics that cmd.exe uses as closely
 * as possible:
 *
 * - Do not search the path if the filename already contains a path (either
 *   relative or absolute).
 *     (but do use path_ext)
 *
 * - If there's really only a filename, check the current directory for file,
 *   then search all path directories.
 *
 * - If filename specifies has *any* extension, search for the file with the
 *   specified extension first.
 *     (not necessary an executable one or one that appears in path_ext;
 *      *but* no extension or just a dot is *not* allowed)
 *
 * - If the literal filename is not found in a directory, try *appending*
 *   (not replacing) extensions from path_ext in the specified order.
 *     (an extension consisting of just a dot *may* appear in path_ext;
 *      unlike what happens if the specified filename ends with a dot,
 *      if path_ext specifies a single dot cmd.exe *does* look for an
 *      extension-less file)
 *
 * - The path variable may contain relative paths; relative paths are relative
 *   to the cwd.
 *
 * - Directories in path may or may not end with a trailing backslash.
 *
 * - Extensions path_ext portions must always start with a dot.
 *
 * - CMD does not trim leading/trailing whitespace from path/pathex entries
 *   nor from the environment variables as a whole.
 *
 * - When cmd.exe cannot read a directory, it wil just skip it and go on
 *   searching. However, unlike posix-y systems, it will happily try to run a
 *   file that is not readable/executable; if the spawn fails it will not
 *   continue searching.
 *
 * TODO: correctly interpret UNC paths
 * TODO: check with cmd what should happen when a pathext entry does not start
 *       with a dot
 */
static inline WCHAR* search_path(const WCHAR *file, WCHAR *cwd,
    const WCHAR *path, const WCHAR *path_ext) {
  WCHAR* result = NULL;

  int file_len = wcslen(file);
  int cwd_len = wcslen(cwd);

  // If the caller supplies an empty filename,
  // we're not gonna return c:\windows\.exe -- GFY!
  if (file_len == 0
      || (file_len == 1 && file[0] == L'.')) {
    return NULL;
  }

  // Find the start of the filename so we can split the directory from the name
  WCHAR *file_name_start;
  for (file_name_start = (WCHAR*)file + file_len;
       file_name_start > file
           && file_name_start[-1] != L'\\'
           && file_name_start[-1] != L'/'
           && file_name_start[-1] != L':';
       file_name_start--);

  bool file_has_dir = file_name_start != file;

  // Check if the filename includes an extension
  WCHAR *dot = wcschr(file_name_start, L'.');
  bool name_has_ext = (dot != NULL && dot[1] != L'\0');

  if (file_has_dir) {
    // The file has a path inside, don't use path (but do use path_ex)
    result = path_search_walk_ext(
        file, file_name_start - file,
        file_name_start, file_len - (file_name_start - file),
        cwd, cwd_len,
        path_ext, name_has_ext);

  } else {
    const WCHAR *dir_start,
                *dir_end = path;

    // The file is really only a name; look in cwd first, then scan path
    result = path_search_walk_ext(L"", 0,
                                  file, file_len,
                                  cwd, cwd_len,
                                  path_ext, name_has_ext);

    while (result == NULL) {
      if (*dir_end == L'\0') {
        break;
      }

      // Skip the separator that dir_end now points to
      if (dir_end != path) {
        dir_end++;
      }

      // Next slice starts just after where the previous one ended
      dir_start = dir_end;

      // Slice until the next ; or \0 is found
      dir_end = wcschr(dir_start, L';');
      if (dir_end == NULL) {
        dir_end = wcschr(dir_start, L'\0');
      }

      // If the slice is zero-length, don't bother
      if (dir_end - dir_start == 0) {
        continue;
      }

      result = path_search_walk_ext(dir_start, dir_end - dir_start,
                                    file, file_len,
                                    cwd, cwd_len,
                                    path_ext, name_has_ext);
    }
  }

  return result;
}


/*
 * Process exit "watcher" functions. It's not like a real libev watcher,
 * it's more like a wrapper around ev_async, and RegisterWaitForSingleObject.
 * And its not generalized, it only works with child processes.
 * BTW there is only one exit watcher that watches all childs!
 */


// Called from either a eio, a wait thread or a callback thread created by a
// wait thread
void ChildProcess::close_stdio_handles(ChildProcess *child) {
  // Before we proceed to synchronize with the main thread, first close
  // the stdio sockets that the child process has used, because it may
  // take some time and would deadlock if done in the main thread.
  for (int i = 0; i < 3; i++) {
    if (!child->got_custom_fds_[i]) {
      shutdown(reinterpret_cast<SOCKET>(child->stdio_handles_[i]), SD_BOTH);
      closesocket(reinterpret_cast<SOCKET>(child->stdio_handles_[i]));
    }
  }
}


// Called from the main thread
void ChildProcess::notify_exit(uv_handle_t* watcher, int status) {
  // Get the child process, then release the lock
  ChildProcess *child = watcher_status.child;

  ReleaseSemaphore(watcher_status.lock, 1, NULL);

  DWORD exit_code = -127;

  EnterCriticalSection(&child->info_lock_);

  // Did the process even start anyway?
  if (child->did_start_) {
    // Process launched, then exited

    // Drop the wait handle
    UnregisterWait(child->wait_handle_);

    // Fetch the process exit code
    if (GetExitCodeProcess(child->process_handle_, &exit_code) == 0) {
      winapi_perror("GetExitCodeProcess");
    }

    // Close and unset the process handle
    CloseHandle(child->process_handle_);
    child->process_handle_ = NULL;
    child->pid_ = 0;
  }

  LeaveCriticalSection(&child->info_lock_);

  child->OnExit(exit_code);
}


// Called from the eio thread
void ChildProcess::notify_spawn_failure(ChildProcess *child) {
  close_stdio_handles(child);

  DWORD result = WaitForSingleObject(watcher_status.lock, INFINITE);
  assert(result == WAIT_OBJECT_0);

  watcher_status.child = child;

  uv_async_send(&watcher_status.async_watcher);
}


// Called from the windows-managed wait thread
void CALLBACK ChildProcess::watch_wait_callback(void *data,
    BOOLEAN didTimeout) {
  assert(didTimeout == FALSE);

  ChildProcess *child = (ChildProcess*)data;

  close_stdio_handles(child);

  // If the main thread is blocked, and more than one child process returns,
  // the wait thread will block as well here. It doesn't matter because the
  // main thread can only do one thing at a time anyway.
  DWORD result = WaitForSingleObject(watcher_status.lock, INFINITE);
  assert(result == WAIT_OBJECT_0);

  watcher_status.child = child;
  uv_async_send(&watcher_status.async_watcher);
}


// Called from the eio thread
inline void ChildProcess::watch(ChildProcess *child) {
  DWORD result = WaitForSingleObject(watcher_status.lock, INFINITE);
  assert(result == WAIT_OBJECT_0);

  // We must retain the lock here because we don't want the RegisterWait
  // to complete before the wait handle is set to the child process.
  RegisterWaitForSingleObject(&child->wait_handle_, child->process_handle_,
      watch_wait_callback, (void*)child, INFINITE,
      WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);

  ReleaseSemaphore(watcher_status.lock, 1, NULL);
}


/*
 * Spawn helper functions
 */


/*
 * Quotes command line arguments
 * Returns a pointer to the end (next char to be written) of the buffer
 */
static inline WCHAR* quote_cmd_arg(WCHAR *source, WCHAR *target,
    WCHAR terminator) {
  int len = wcslen(source),
      i;

  // Check if the string must be quoted;
  // if unnecessary, don't do it, it may only confuse older programs.
  if (len == 0) {
    goto quote;
  }
  for (i = 0; i < len; i++) {
    if (source[i] == L' ' || source[i] == L'"') {
      goto quote;
    }
  }

  // No quotation needed
  wcsncpy(target, source, len);
  target += len;
  *(target++) = terminator;
  return target;

quote:
  // Quote
  *(target++) = L'"';
  for (i = 0; i < len; i++) {
    if (source[i] == L'"' || source[i] == L'\\') {
      *(target++) = '\\';
    }
    *(target++) = source[i];
  }
  *(target++) = L'"';
  *(target++) = terminator;

  return target;
}


/*
 * Spawns a child process from a libeio thread
 */
int ChildProcess::do_spawn(eio_req *req) {
  ChildProcess* child = (ChildProcess*)req->data;

  WCHAR* application_path = search_path(child->application_, child->cwd_,
      child->path_, child->path_ext_);

  if (application_path) {
    STARTUPINFOW startup;
    PROCESS_INFORMATION info;

    startup.cb = sizeof(startup);
    startup.lpReserved = NULL;
    startup.lpDesktop = NULL;
    startup.lpTitle = NULL;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.cbReserved2 = 0;
    startup.lpReserved2 = NULL;
    startup.hStdInput = child->stdio_handles_[0];
    startup.hStdOutput = child->stdio_handles_[1];
    startup.hStdError = child->stdio_handles_[2];

    EnterCriticalSection(&child->info_lock_);

    if (!child->kill_me_) {
      // Try start the process
      BOOL success = CreateProcessW(
        application_path,
        child->arguments_,
        NULL,
        NULL,
        1,
        CREATE_UNICODE_ENVIRONMENT,
        child->env_win_,
        child->cwd_,
        &startup,
        &info
      );

      if (success) {
        child->process_handle_ = info.hProcess;
        child->pid_ = GetProcessId(info.hProcess);
        child->did_start_ = true;
        watch(child);

        // Not interesting
        CloseHandle(info.hThread);

        LeaveCriticalSection(&child->info_lock_);
        return 0;
      }
    }

    LeaveCriticalSection(&child->info_lock_);
  }

  // not found, kill_me set or process failed to start
  notify_spawn_failure(child);
  return 0;
}


// Called from the main thread after spawn has finished,
// there's no need to lock the child because did_start is reliable
int ChildProcess::after_spawn(eio_req *req) {
  ChildProcess* child = (ChildProcess*)req->data;

  if (child->did_start_) {
    child->handle_->Set(pid_symbol, Integer::New(child->pid_));
  } else {
    child->handle_->Set(pid_symbol, Local<Value>::New(Null()));
  }

  // Cleanup data structures needed only for spawn() here
  delete [] child->application_;
  delete [] child->arguments_;
  delete [] child->env_win_;
  delete [] child->cwd_;

  return 0;
}


/*
 * Kill helper functions
 */

// Called from the main thread while eio/wait threads may still be busy with
// the process
int ChildProcess::do_kill(ChildProcess *child, int sig) {
  int rv = 0;

  EnterCriticalSection(&child->info_lock_);

  child->exit_signal_ = sig;

  if (child->did_start_) {
    // On windows killed processes normally return 1
    if (!TerminateProcess(child->process_handle_, 1))
      rv = -1;
  } else {
    child->kill_me_ = true;
  }

  LeaveCriticalSection(&child->info_lock_);

  return rv;
}


/*
 * ChildProcess non-static Methods
 */

Handle<Value> ChildProcess::New(const Arguments& args) {
  HandleScope scope;
  ChildProcess *p = new ChildProcess();
  p->Wrap(args.Holder());
  return args.This();
}


// This is an internal function. The third argument should be an array
// of key value pairs seperated with '='.
Handle<Value> ChildProcess::Spawn(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3 ||
      !args[0]->IsString() ||
      !args[1]->IsArray() ||
      !args[2]->IsString() ||
      !args[3]->IsArray()) {
    return ThrowException(Exception::Error(String::New("Bad argument.")));
  }

  // Get ChildProcess object
  ChildProcess *child = ObjectWrap::Unwrap<ChildProcess>(args.Holder());

  // Copy appplication name
  Handle<String> app_handle = args[0]->ToString();
  int app_len = app_handle->Length();
  String::Value app(app_handle);
  child->application_ = new WCHAR[app_len + 1];
  wcsncpy(child->application_, (WCHAR*)*app, app_len + 1);

  /*
   * Copy second argument args[1] into a c-string called argv.
   * On windows command line arguments are all quoted and concatenated to
   * one string. The executable name must be prepended. This is not really
   * required by windows but if you don't do this programs that rely on
   * argv[0] being the executable misbehave.
   * Assuming that executable plus all arguments must be wrapped in quotes,
   * every character needs to be quoted with a backslash,
   * and every argument is followed by either a space or a nul char,
   * the maximum required buffer size is Σ[exe and args](2 * length + 3).
   */
  Local<Array> cmd_args_handle = Local<Array>::Cast(args[1]);
  int cmd_argc = cmd_args_handle->Length();

  // Compute required buffer
  int max_buf = (1 + cmd_argc) * 3 + app_len * 2,
      i;
  for (i = 0; i < cmd_argc; i++) {
    Local<String> arg_handle =
        cmd_args_handle->Get(Integer::New(i))->ToString();
    max_buf += arg_handle->Length() * 2;
  }

  child->arguments_ = new WCHAR[max_buf];
  WCHAR *pos = child->arguments_;
  pos = quote_cmd_arg((WCHAR*)*app, pos, cmd_argc ? L' ' : L'\0');
  for (i = 0; i < cmd_argc; i++) {
    String::Value arg(cmd_args_handle->Get(Integer::New(i))->ToString());
    pos = quote_cmd_arg((WCHAR*)*arg, pos, (i < cmd_argc - 1) ? L' ' : L'\0');
  }

  // Current working directory
  Local<String>cwd_handle = Local<String>::Cast(args[2]);
  int cwd_len = cwd_handle->Length();
  if (cwd_len > 0) {
    // Cwd was specified
    String::Value cwd(cwd_handle);
    child->cwd_ = new WCHAR[cwd_len + 1];
    wcsncpy(child->cwd_, (WCHAR*)*cwd, cwd_len + 1);
  } else {
    // Cwd not specified
    int chars = GetCurrentDirectoryW(0, NULL);
    if (!chars) {
      winapi_perror("GetCurrentDirectoryW");
      child->cwd_ = new WCHAR[0];
      child->cwd_[0] = '\0';
    } else {
      child->cwd_ = new WCHAR[chars];
      GetCurrentDirectoryW(chars, child->cwd_);
    }
  }

  /*
   * args[3] holds the environment as a js array containing key=value pairs.
   * The way windows takes environment variables is different than what C does;
   * Windows wants a contiguous block of null-terminated strings, terminated
   * with an additional null.
   * Get a pointer to the pathext and path environment variables as well,
   * because do_spawn needs it. These are just pointers into env_win.
   */
  Local<Array> env_list_handle = Local<Array>::Cast(args[3]);
  int envc = env_list_handle->Length();
  Local<String> env_val_handle[envc];

  int env_win_len = envc + 1; // room for \0 terminators plus closing null
  for (int i = 0; i < envc; i++) {
    env_val_handle[i] = env_list_handle->Get(Integer::New(i))->ToString();
    env_win_len += env_val_handle[i]->Length();
  }

  WCHAR *env_win = new WCHAR[env_win_len],
        *env_win_pos = env_win;
  WCHAR *path = NULL, *path_ext = NULL;

  for (int i = 0; i < envc; i++) {
    int len = env_val_handle[i]->Length() + 1; // including \0
    String::Value pair(env_val_handle[i]);
    wcsncpy(env_win_pos, (WCHAR*)*pair, (size_t)len);

    // Try to get a pointer to PATH and PATHEXT
    if (_wcsnicmp(L"PATH=", env_win_pos, 5) == 0) {
      path = env_win_pos + 5;
    }
    if (_wcsnicmp(L"PATHEXT=", env_win_pos, 8) == 0) {
      path_ext = env_win_pos + 8;
    }

    env_win_pos += len;
  }

  *env_win_pos = L'\0';

  child->env_win_ = env_win;

  if (path != NULL) {
    child->path_ = path;
  } else {
    child->path_ = DEFAULT_PATH;
  }

  if (path_ext != NULL) {
    child->path_ext_ = path_ext;
  } else {
    child->path_ext_ = DEFAULT_PATH_EXT;
  }

  // Open pipes or re-use custom_fds to talk to child
  Local<Array> custom_fds_handle = Local<Array>::Cast(args[4]);
  int custom_fds_len = custom_fds_handle->Length();

  HANDLE *child_handles = (HANDLE*)&child->stdio_handles_;
  bool *has_custom_fds = (bool*)&child->got_custom_fds_;
  int parent_fds[3];

  for (int i = 0; i < 3; i++) {
    int custom_fd = -1;
    if (i < custom_fds_len && !custom_fds_handle->Get(i)->IsUndefined())
      custom_fd = custom_fds_handle->Get(i)->ToInteger()->Value();

    if (custom_fd == -1) {
      // Create a new pipe
      HANDLE parent_handle, child_handle;
      if (wsa_sync_async_socketpair(AF_INET, SOCK_STREAM, IPPROTO_IP,
          (SOCKET*)&child_handle, (SOCKET*)&parent_handle) == SOCKET_ERROR)
        wsa_perror("wsa_sync_async_socketpair");

      // Make parent handle nonblocking
      unsigned long ioctl_value = 1;
      if (ioctlsocket((SOCKET)parent_handle, FIONBIO, &ioctl_value) ==
          SOCKET_ERROR)
        wsa_perror("ioctlsocket");

      // Make parent handle non-inheritable
      if (!SetHandleInformation(parent_handle, HANDLE_FLAG_INHERIT, 0))
        winapi_perror("SetHandleInformation");

      // Make child handle inheritable
      if (!SetHandleInformation(child_handle, HANDLE_FLAG_INHERIT,
          HANDLE_FLAG_INHERIT))
        winapi_perror("SetHandleInformation");

      // Enable linger on socket so all written data gets through
      BOOL opt_value = 0;
      if (setsockopt((SOCKET)child_handle, SOL_SOCKET, SO_DONTLINGER,
          (char*)&opt_value, sizeof(opt_value)) == SOCKET_ERROR)
        wsa_perror("setsockopt");

      has_custom_fds[i] = false;
      child_handles[i] = child_handle;
      parent_fds[i] = (int)_open_osfhandle((intptr_t)parent_handle, 0);

    } else {
      // Use this custom fd
      HANDLE custom_handle = (HANDLE)_get_osfhandle(custom_fd);

      // Make handle inheritable, don't care it it fails
      // It may fail for certain types of handles - but always try to
      // spawn; it'll still work for e.g. console handles
      SetHandleInformation(custom_handle, HANDLE_FLAG_INHERIT,
          HANDLE_FLAG_INHERIT);

      has_custom_fds[i] = true;
      child_handles[i] = custom_handle;
      parent_fds[i] = custom_fd;
    }
  }

  // Return the opened fds
  Local<Array> result = Array::New(3);
  assert(parent_fds[0] >= 0);
  result->Set(0, Integer::New(parent_fds[0]));
  assert(parent_fds[1] >= 0);
  result->Set(1, Integer::New(parent_fds[1]));
  assert(parent_fds[2] >= 0);
  result->Set(2, Integer::New(parent_fds[2]));

  // Grab a reference so it doesn't get GC'ed
  child->Ref();

  eio_custom(do_spawn, EIO_PRI_DEFAULT, after_spawn, (void*)child);

  return scope.Close(result);
}


Handle<Value> ChildProcess::Kill(const Arguments& args) {
  HandleScope scope;
  ChildProcess *child = ObjectWrap::Unwrap<ChildProcess>(args.Holder());
  assert(child);

  int sig = SIGTERM;

  if (args.Length() > 0) {
    if (args[0]->IsNumber()) {
      sig = args[0]->Int32Value();
    } else {
      return ThrowException(Exception::TypeError(String::New("Bad argument.")));
    }
  }

  if (do_kill(child, sig) != 0) {
    return ThrowException(ErrnoException(GetLastError()));
  }

  return True();
}


// Called from the main thread _after_ all eio/wait threads are done with the
// process, so there's no need to lock here.
void ChildProcess::OnExit(int status) {
  HandleScope scope;

  // Unref() the child, as it's no longer used by threads
  Unref();

  handle_->Set(pid_symbol, Null());

  Local<Value> onexit_v = handle_->Get(onexit_symbol);
  assert(onexit_v->IsFunction());
  Local<Function> onexit = Local<Function>::Cast(onexit_v);

  TryCatch try_catch;
  Local<Value> argv[2];

  argv[0] = Integer::New(status);

  if (exit_signal_ != 0) {
    argv[1] = Integer::New(exit_signal_);
  } else {
    argv[1] = Local<Value>::New(Null());
  }

  onexit->Call(handle_, 2, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


void ChildProcess::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(ChildProcess::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("ChildProcess"));

  pid_symbol = NODE_PSYMBOL("pid");
  onexit_symbol = NODE_PSYMBOL("onexit");

  NODE_SET_PROTOTYPE_METHOD(t, "spawn", ChildProcess::Spawn);
  NODE_SET_PROTOTYPE_METHOD(t, "kill", ChildProcess::Kill);

  target->Set(String::NewSymbol("ChildProcess"), t->GetFunction());

  uv_async_init(&watcher_status.async_watcher, notify_exit, NULL, NULL);
  watcher_status.lock = CreateSemaphore(NULL, 1, 1, NULL);
}

}  // namespace node

NODE_MODULE(node_child_process, node::ChildProcess::Initialize);
