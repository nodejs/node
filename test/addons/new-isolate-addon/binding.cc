#include <assert.h>
#include <node.h>

using node::Environment;
using node::MultiIsolatePlatform;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::MaybeLocal;
using v8::Object;
using v8::SharedArrayBuffer;
using v8::String;
using v8::Unlocker;
using v8::Value;

void RunInSeparateIsolate(const FunctionCallbackInfo<Value>& args) {
  Isolate* parent_isolate = args.GetIsolate();

  assert(args[0]->IsString());
  String::Utf8Value code(parent_isolate, args[0]);
  assert(*code != nullptr);
  assert(args[1]->IsSharedArrayBuffer());
  auto arg_bs = args[1].As<SharedArrayBuffer>()->GetBackingStore();

  Environment* parent_env =
      node::GetCurrentEnvironment(parent_isolate->GetCurrentContext());
  assert(parent_env != nullptr);
  MultiIsolatePlatform* platform = node::GetMultiIsolatePlatform(parent_env);
  assert(parent_env != nullptr);

  {
    Unlocker unlocker(parent_isolate);

    std::vector<std::string> errors;
    const std::vector<std::string> empty_args;
    auto setup =
        node::CommonEnvironmentSetup::Create(platform,
                                             &errors,
                                             empty_args,
                                             empty_args,
                                             node::EnvironmentFlags::kNoFlags);
    assert(setup);

    {
      Locker locker(setup->isolate());
      Isolate::Scope isolate_scope(setup->isolate());
      HandleScope handle_scope(setup->isolate());
      Context::Scope context_scope(setup->context());
      auto arg = SharedArrayBuffer::New(setup->isolate(), arg_bs);
      auto result = setup->context()->Global()->Set(
          setup->context(),
          v8::String::NewFromUtf8Literal(setup->isolate(), "arg"),
          arg);
      assert(!result.IsNothing());

      MaybeLocal<Value> loadenv_ret =
          node::LoadEnvironment(setup->env(), *code);
      assert(!loadenv_ret.IsEmpty());

      (void)node::SpinEventLoop(setup->env());

      node::Stop(setup->env());
    }
  }
}

void Initialize(Local<Object> exports,
                Local<Value> module,
                Local<Context> context) {
  NODE_SET_METHOD(exports, "runInSeparateIsolate", RunInSeparateIsolate);
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
