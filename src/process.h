#ifndef node_process_h
#define node_process_h

#include "node.h"
#include <v8.h>
#include <ev.h>
#include <oi_socket.h>

namespace node {

class Process : ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);
  virtual size_t size (void) { return sizeof(Process); }

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Write (const v8::Arguments& args);
  static v8::Handle<v8::Value> Close (const v8::Arguments& args);
  static v8::Handle<v8::Value> Kill (const v8::Arguments& args);
  static v8::Handle<v8::Value> PIDGetter (v8::Local<v8::String> _, const v8::AccessorInfo& info);

  Process(v8::Handle<v8::Object> handle);
  ~Process();

  void Shutdown ();
  int Spawn (const char *command);
  int Write (oi_buf *buf);
  int Close ();
  int Kill (int sig);

 private:
  static void OnOutput (EV_P_ ev_io *watcher, int revents);
  static void OnError (EV_P_ ev_io *watcher, int revents);
  static void OnWritable (EV_P_ ev_io *watcher, int revents);
  static void OnExit (EV_P_ ev_child *watcher, int revents);

  ev_io stdout_watcher_;
  ev_io stderr_watcher_;
  ev_io stdin_watcher_;
  ev_child child_watcher_;

  int stdout_pipe_[2];
  int stderr_pipe_[2];
  int stdin_pipe_[2];

  pid_t pid_;

  bool got_close_;

  oi_queue out_stream_;
};

} // namespace node 
#endif //  node_process_h
