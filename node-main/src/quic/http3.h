#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <base_object.h>
#include <env.h>
#include <memory_tracker.h>
#include "session.h"

namespace node::quic {
// Provides an implementation of the HTTP/3 Application implementation
class Http3Application final : public Session::ApplicationProvider {
 public:
  JS_CONSTRUCTOR(Http3Application);
  JS_BINDING_INIT_BOILERPLATE();

  Http3Application(Environment* env,
                   v8::Local<v8::Object> object,
                   const Session::Application_Options& options);

  std::unique_ptr<Session::Application> Create(Session* session) override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(Http3Application)
  SET_MEMORY_INFO_NAME(Http3Application)

  std::string ToString() const;

 private:
  JS_METHOD(New);

  Session::Application_Options options_;
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
