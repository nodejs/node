#include <libplatform/libplatform.h>
#include <v8-cppgc.h>
#include <v8.h>

#include <cppgc/allocation.h>
#include <cppgc/default-platform.h>
#include <cppgc/garbage-collected.h>
#include <cppgc/heap.h>
#include <cppgc/member.h>
#include <cppgc/platform.h>
#include <cppgc/visitor.h>

class Wrappable : public v8::Object::Wrappable {
 public:
  void Trace(cppgc::Visitor* visitor) const override {
    v8::Object::Wrappable::Trace(visitor);
  }
};

int main(int argc, char* argv[]) {
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  cppgc::InitializeProcess(platform->GetPageAllocator());
  v8::V8::Initialize();

  auto heap = v8::CppHeap::Create(platform.get(), v8::CppHeapCreateParams{{}});
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  create_params.cpp_heap = heap.release();

  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    Wrappable* wrappable = cppgc::MakeGarbageCollected<Wrappable>(
        isolate->GetCppHeap()->GetAllocationHandle());
    v8::Object::Wrap<v8::CppHeapPointerTag::kDefaultTag>(
        isolate, obj, wrappable);
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8Literal(isolate, "'Hello' + ', World!'");
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source).ToLocalChecked();
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
    v8::String::Utf8Value utf8(isolate, result);
    printf("%s\n", *utf8);
  }

  isolate->Dispose();
  cppgc::ShutdownProcess();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  delete create_params.array_buffer_allocator;

  return 0;
}
