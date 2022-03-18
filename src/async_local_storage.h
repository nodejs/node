// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_ASYNC_LOCAL_STORAGE_H_
#define SRC_ASYNC_LOCAL_STORAGE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "context_storage.h"
#include "base_object.h"
#include "v8.h"

#include <cstdint>

namespace node {

class Environment;
class ExternalReferenceRegistry;

class AsyncLocalStorageState : public BaseObject {
  friend class AsyncLocalStorage;

 public:
  AsyncLocalStorageState(Environment* env, v8::Local<v8::Object> wrap);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static bool HasInstance(Environment* env,
                          const v8::Local<v8::Value>& object);

  static BaseObjectPtr<AsyncLocalStorageState> Create(Environment* env);

  static void Restore(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Clear(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(AsyncLocalStorageState)
  SET_SELF_SIZE(AsyncLocalStorageState)

 private:
  std::shared_ptr<ContextStorageState> state;
};

class AsyncLocalStorage : public BaseObject {
 public:
  // This constructor creates a reusable instance where user is responsible
  // to call set_provider_type() and AsyncReset() before use.
  AsyncLocalStorage(Environment* env, v8::Local<v8::Object> object);
  AsyncLocalStorage() = delete;

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context,
                         void* priv);

  static void EnterWith(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Exit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetStore(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Run(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void StoreState(v8::Local<v8::Object> resource);
  static std::shared_ptr<ContextStorageState> GetState(
      v8::Local<v8::Object> resource);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(AsyncLocalStorage)
  SET_SELF_SIZE(AsyncLocalStorage)

 private:
  std::shared_ptr<ContextStorage> storage_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ASYNC_LOCAL_STORAGE_H_
