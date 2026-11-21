#include <assert.h>
#include <cppgc/allocation.h>
#include <cppgc/garbage-collected.h>
#include <cppgc/heap.h>
#include <node.h>
#include <v8-cppgc.h>
#include <v8-sandbox.h>
#include <v8.h>
#include <algorithm>

class CppGCed : public v8::Object::Wrappable {
 public:
  static uint16_t* states;
  static constexpr int kDestructCount = 0;
  static constexpr int kTraceCount = 1;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Object> js_object = args.This();
    auto* heap = isolate->GetCppHeap();
    assert(heap != nullptr);
    CppGCed* gc_object =
        cppgc::MakeGarbageCollected<CppGCed>(heap->GetAllocationHandle());
    node::SetCppgcReference(isolate, js_object, gc_object);
    args.GetReturnValue().Set(js_object);
  }

  static v8::Local<v8::Function> GetConstructor(
      v8::Local<v8::Context> context) {
    auto ft = v8::FunctionTemplate::New(v8::Isolate::GetCurrent(), New);
    return ft->GetFunction(context).ToLocalChecked();
  }

  CppGCed() = default;

  ~CppGCed() { states[kDestructCount]++; }

  void Trace(cppgc::Visitor* visitor) const { states[kTraceCount]++; }
};

uint16_t* CppGCed::states = nullptr;

void InitModule(v8::Local<v8::Object> exports) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  // Let V8 allocate the counters, as the V8 sandbox does not allow external
  // backing stores. The store is leaked so that the counters outlive the
  // CppHeap, whose teardown runs the destructors.
  static auto* store = new std::shared_ptr<v8::BackingStore>(
      v8::ArrayBuffer::NewBackingStore(isolate, sizeof(uint16_t) * 2));
  CppGCed::states = static_cast<uint16_t*>((*store)->Data());
  std::fill_n(CppGCed::states, 2, 0);
  auto ab = v8::ArrayBuffer::New(isolate, *store);

  exports
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "CppGCed").ToLocalChecked(),
            CppGCed::GetConstructor(context))
      .FromJust();
  exports
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "states").ToLocalChecked(),
            v8::Uint16Array::New(ab, 0, 2))
      .FromJust();
  exports
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "kDestructCount").ToLocalChecked(),
            v8::Integer::New(isolate, CppGCed::kDestructCount))
      .FromJust();
  exports
      ->Set(context,
            v8::String::NewFromUtf8(isolate, "kTraceCount").ToLocalChecked(),
            v8::Integer::New(isolate, CppGCed::kTraceCount))
      .FromJust();
}

NODE_MODULE(NODE_GYP_MODULE_NAME, InitModule)
