#include "base_object-inl.h"
#include "node_binding.h"
#include "node_test_fixture.h"
#include "node_v8.h"

#include <string>

using node::v8_utils::GCProfiler;

namespace {

v8::Local<v8::Value> Get(v8::Local<v8::Context> context,
                         v8::Local<v8::Object> object,
                         const char* name) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  return object
      ->Get(context, v8::String::NewFromUtf8(isolate, name).ToLocalChecked())
      .ToLocalChecked();
}

void CallMethod(v8::Local<v8::Context> context,
                v8::Local<v8::Object> object,
                const char* name) {
  Get(context, object, name)
      .As<v8::Function>()
      ->Call(context, object, 0, nullptr)
      .ToLocalChecked();
}

class GCProfilerTest : public EnvironmentTestFixture {};

// Refs: https://github.com/nodejs/node/issues/67790
TEST_F(GCProfilerTest, StopsRecordingAfterStop) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();
  node::LoadEnvironment(*test_env, "");

  v8::Local<v8::Function> get_internal_binding =
      v8::Function::New(context, node::binding::GetInternalBinding)
          .ToLocalChecked();
  v8::Local<v8::Value> binding_name =
      v8::String::NewFromUtf8Literal(isolate_, "v8");
  v8::Local<v8::Object> binding =
      get_internal_binding
          ->Call(context, v8::Undefined(isolate_), 1, &binding_name)
          .ToLocalChecked()
          .As<v8::Object>();

  v8::Local<v8::Object> object = Get(context, binding, "GCProfiler")
                                     .As<v8::Function>()
                                     ->NewInstance(context)
                                     .ToLocalChecked();
  GCProfiler* profiler = node::BaseObject::Unwrap<GCProfiler>(object);
  ASSERT_NE(profiler, nullptr);

  CallMethod(context, object, "start");
  isolate_->LowMemoryNotification();
  CallMethod(context, object, "stop");
  const size_t profile_size = profiler->out_stream()->str().size();
  EXPECT_NE(profiler->out_stream()->str().find("\"gcType\""),
            std::string::npos);

  // `object` is still reachable here, so the profiler is not destroyed and
  // only stop() can have removed its GC callbacks.
  isolate_->LowMemoryNotification();
  EXPECT_EQ(profiler->out_stream()->str().size(), profile_size);
}

}  // namespace
