#ifndef node_timer_h
#define node_timer_h

#include "node.h"
#include <v8.h>
#include <ev.h>

namespace node {

class Timer : ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);

 protected:
  Timer(v8::Handle<v8::Object> handle,
        v8::Handle<v8::Function> callback,
        ev_tstamp after,
        ev_tstamp repeat);
  ~Timer();

  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> Start (const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop (const v8::Arguments& args);

 private:
  static void OnTimeout (EV_P_ ev_timer *watcher, int revents);
  ev_timer watcher_;
};

} // namespace node 
#endif //  node_timer_h
