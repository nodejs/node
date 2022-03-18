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

#ifndef SRC_CONTEXT_STORAGE_H_
#define SRC_CONTEXT_STORAGE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>

#include "v8.h"

namespace node {

class Environment;
class ExternalReferenceRegistry;

class ContextStorage {
  friend class ContextStorageState;

 private:
  v8::Isolate* isolate_;
  v8::Global<v8::Value> stored;

  void Enter(const v8::PersistentBase<v8::Value>& data);

 public:
  ContextStorage(v8::Isolate* isolate);
  ContextStorage();
  ~ContextStorage();

  static std::shared_ptr<ContextStorage> Create(v8::Isolate* isolate);
  static std::shared_ptr<ContextStorage> Create();

  bool Enabled();
  void Enable();
  void Disable();
  void Enter(v8::Local<v8::Value> data);
  void Exit();
  v8::Local<v8::Value> Get();
};

class ContextStorageState {
 private:
  std::unordered_map<std::shared_ptr<ContextStorage>, v8::Global<v8::Value>> state;

 public:
  void Restore();
  void Clear();
  static std::shared_ptr<ContextStorageState> State();
};

class ContextStorageRunScope {
 private:
  std::shared_ptr<ContextStorage> storage_;

 public:
  ContextStorageRunScope(std::shared_ptr<ContextStorage> storage,
                         v8::Local<v8::Value> data);
  ~ContextStorageRunScope();
};

class ContextStorageExitScope {
 private:
  std::shared_ptr<ContextStorage> storage_;

 public:
  ContextStorageExitScope(std::shared_ptr<ContextStorage> storage);
  ~ContextStorageExitScope();
};

class ContextStorageRestoreScope {
 private:
  std::shared_ptr<ContextStorageState> state;

 public:
  ContextStorageRestoreScope(std::shared_ptr<ContextStorageState> state);
  ~ContextStorageRestoreScope();
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CONTEXT_STORAGE_H_
