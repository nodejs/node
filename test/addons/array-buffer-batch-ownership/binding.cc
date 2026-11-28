#include <node.h>
#include <v8.h>

#include <memory>

namespace {

bool SameOwner(const std::weak_ptr<v8::BackingStore>& previous,
               const std::shared_ptr<v8::BackingStore>& current) {
  return !previous.owner_before(current) && !current.owner_before(previous);
}

void CheckOwnership(const v8::FunctionCallbackInfo<v8::Value>& args) {
  auto* isolate = args.GetIsolate();
  auto context = isolate->GetCurrentContext();
  auto source = args[0].As<v8::ArrayBuffer>();
  auto strong = source->GetBackingStore();
  std::weak_ptr<v8::BackingStore> previous = strong;
  auto alias = v8::ArrayBuffer::New(isolate, strong);
  if (!SameOwner(previous, source->GetBackingStore()) ||
      !SameOwner(previous, alias->GetBackingStore())) {
    return;
  }

  v8::Local<v8::Value> argv[] = {alias};
  auto receiver = v8::Undefined(isolate);
  if (args[1].As<v8::Function>()->Call(context, receiver, 1, argv).IsEmpty())
    return;
  if (!source->WasDetached() || !alias->WasDetached() || previous.expired())
    return;
  strong.reset();
  if (!previous.expired()) return;

  v8::Local<v8::Value> next;
  if (!args[2]
           .As<v8::Function>()
           ->Call(context, receiver, 0, nullptr)
           .ToLocal(&next)) {
    return;
  }
  auto current = next.As<v8::ArrayBuffer>()->GetBackingStore();
  args.GetReturnValue().Set(!SameOwner(previous, current));
}

NODE_MODULE_INIT() {
  NODE_SET_METHOD(exports, "checkOwnership", CheckOwnership);
}

}  // namespace
