#include "node.h"
#include "v8.h"
#include "v8-profiler.h"

namespace {

inline void Test(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* const isolate = args.GetIsolate();
  const v8::HeapSnapshot* const heap_snapshot =
      isolate->GetHeapProfiler()->TakeHeapSnapshot();
  struct : public v8::OutputStream {
    WriteResult WriteAsciiChunk(char*, int) override { return kContinue; }
    void EndOfStream() override {}
  } output_stream;
  heap_snapshot->Serialize(&output_stream, v8::HeapSnapshot::kJSON);
  const_cast<v8::HeapSnapshot*>(heap_snapshot)->Delete();
}

inline void Initialize(v8::Local<v8::Object> binding) {
  v8::Isolate* const isolate = binding->GetIsolate();
  binding->Set(v8::String::NewFromUtf8(isolate, "test"),
               v8::FunctionTemplate::New(isolate, Test)->GetFunction());
}

NODE_MODULE(test, Initialize)

}  // anonymous namespace
