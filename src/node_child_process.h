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

#ifndef NODE_CHILD_PROCESS_H_
#define NODE_CHILD_PROCESS_H_

#include <node.h>
#include <node_object_wrap.h>

#include <v8.h>
#include <uv.h>

#ifdef __POSIX__
# include <ev.h>
#endif

#ifdef __MINGW32__
# include <platform_win32.h> // HANDLE type
#endif

// ChildProcess is a thin wrapper around ev_child. It has the extra
// functionality that it can spawn a child process with pipes connected to
// its stdin, stdout, stderr. This class is not meant to be exposed to but
// wrapped up in a more friendly EventEmitter with streams for each of the
// pipes.
//
// When the child process exits (when the parent receives SIGCHLD) the
// callback child.onexit will be called.

namespace node {

class ChildProcess : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Spawn(const v8::Arguments& args);
  static v8::Handle<v8::Value> Kill(const v8::Arguments& args);

  ChildProcess() : ObjectWrap() {
#ifdef __POSIX__
    ev_init(&child_watcher_, ChildProcess::on_chld);
    child_watcher_.data = this;
#endif // __POSIX__

    pid_ = -1;

#ifdef __MINGW32__
    InitializeCriticalSection(&info_lock_);
    kill_me_ = false;
    did_start_ = false;
    exit_signal_ = 0;
#endif // __MINGW32__
  }

  ~ChildProcess() {
#ifdef __POSIX__
    Stop();
#endif // __POSIX__
  }

  // Returns 0 on success. stdio_fds will contain file desciptors for stdin,
  // stdout, and stderr of the subprocess. stdin is writable; the other two
  // are readable.
  // The user of this class has responsibility to close these pipes after
  // the child process exits.
  int Spawn(const char *file,
            char *const argv[],
            const char *cwd,
            char **env,
            int stdio_fds[3],
            int custom_fds[3],
            bool do_setsid,
            int custom_uid,
            char *custom_uname,
            int custom_gid,
            char *custom_gname,
            int* channel);

  // Simple syscall wrapper. Does not disable the watcher. onexit will be
  // called still.
  int Kill(int sig);

 private:
  void OnExit(int code);

#ifdef __POSIX__ // Shouldn't this just move to node_child_process.cc?
  void Stop(void);

  static void on_chld(EV_P_ ev_child *watcher, int revents) {
    ChildProcess *child = static_cast<ChildProcess*>(watcher->data);
    assert(revents == EV_CHILD);
    assert(child->pid_ == watcher->rpid);
    assert(&child->child_watcher_ == watcher);
    child->OnExit(watcher->rstatus);
  }

  ev_child child_watcher_;
  pid_t pid_;
#endif // __POSIX__

#ifdef __MINGW32__
  static int do_spawn(eio_req *req);
  static int after_spawn(eio_req *req);
  static void watch(ChildProcess *child);
  static void CALLBACK watch_wait_callback(void *data, BOOLEAN didTimeout);
  static void notify_spawn_failure(ChildProcess *child);
  static void notify_exit(uv_handle_t* watcher, int status);
  static int do_kill(ChildProcess *child, int sig);static void close_stdio_handles(ChildProcess *child);

  int pid_;
  int exit_signal_;

  WCHAR *application_;
  WCHAR *arguments_;
  WCHAR *env_win_;
  WCHAR *cwd_;
  const WCHAR *path_;
  const WCHAR *path_ext_;

  HANDLE stdio_handles_[3];
  bool got_custom_fds_[3];

  CRITICAL_SECTION info_lock_;
  bool did_start_;
  bool kill_me_;
  HANDLE wait_handle_;
  HANDLE process_handle_;
#endif // __MINGW32__
};

}  // namespace node
#endif  // NODE_CHILD_PROCESS_H_
