// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_CHILD_PROCESS_H_
#define SRC_CHILD_PROCESS_H_

#include <node.h>
#include <node_events.h>

#include <v8.h>
#include <ev.h>
#include <evcom.h>

namespace node {

class ChildProcess : EventEmitter {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Spawn(const v8::Arguments& args);
  static v8::Handle<v8::Value> Write(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> Kill(const v8::Arguments& args);
  static v8::Handle<v8::Value> PIDGetter(v8::Local<v8::String> _,
                                         const v8::AccessorInfo& info);

  ChildProcess();
  ~ChildProcess();

  int Spawn(const char *file, char *const argv[], char **env);
  int Write(const char *str, size_t len);
  int Close(void);
  int Kill(int sig);

 private:
  static void on_read(evcom_reader *r, const void *buf, size_t len);
  static void reader_closed(evcom_reader *r);
  static void stdin_closed(evcom_writer *w);
  static void OnCHLD(EV_P_ ev_child *watcher, int revents);

  void MaybeShutdown(void);
  void Shutdown(void);

  evcom_reader stdout_reader_;
  evcom_reader stderr_reader_;
  evcom_writer stdin_writer_;

  ev_child child_watcher_;

  int stdout_fd_;
  int stderr_fd_;
  int stdin_fd_;

  enum encoding stdout_encoding_;
  enum encoding stderr_encoding_;

  pid_t pid_;

  bool got_chld_;
  int exit_code_;
};

}  // namespace node
#endif  // SRC_CHILD_PROCESS_H_
