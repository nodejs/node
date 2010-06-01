// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef NODE_CHILD_PROCESS_H_
#define NODE_CHILD_PROCESS_H_

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <ev.h>

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
    ev_init(&child_watcher_, ChildProcess::on_chld);
    child_watcher_.data = this;
    pid_ = -1;
  }

  ~ChildProcess() {
    Stop();
  }

  // Returns 0 on success. stdio_fds will contain file desciptors for stdin,
  // stdout, and stderr of the subprocess. stdin is writable; the other two
  // are readable.
  // The user of this class has responsibility to close these pipes after
  // the child process exits.
  int Spawn(const char *file, char *const argv[], char **env, int stdio_fds[3], int custom_fds[3]);

  // Simple syscall wrapper. Does not disable the watcher. onexit will be
  // called still.
  int Kill(int sig);

 private:
  void OnExit(int code);
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
};

}  // namespace node
#endif  // NODE_CHILD_PROCESS_H_
