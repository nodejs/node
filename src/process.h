#ifndef node_process_h
#define node_process_h

#include "node.h"
#include <v8.h>
#include <ev.h>

namespace node {

class Process : ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);

  virtual size_t size (void) { return sizeof(Process); }

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New (const v8::Arguments& args);

  Process(v8::Handle<v8::Object> handle);
  ~Process();

  void Shutdown ();
  int Spawn (const char *command);

 private:
  static void OnOutput (EV_P_ ev_io *watcher, int revents);
  static void OnError (EV_P_ ev_io *watcher, int revents);
  static void OnWritable (EV_P_ ev_io *watcher, int revents);
  static void OnCHLD (EV_P_ ev_child *watcher, int revents);

  ev_io stdout_watcher_;
  ev_io stderr_watcher_;
  ev_io stdin_watcher_;
  ev_child child_watcher_;

  int stdout_pipe_[2];
  int stderr_pipe_[2];
  int stdin_pipe_[2];

  pid_t pid_;
};

} // namespace node 
#endif //  node_process_h
