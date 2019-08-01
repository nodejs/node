// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_C_API_H_
#define V8_WASM_C_API_H_

#include "include/v8.h"
#include "src/common/globals.h"
#include "third_party/wasm-api/wasm.hh"

namespace wasm {

class StoreImpl {
 public:
  ~StoreImpl();

  v8::Isolate* isolate() const { return isolate_; }
  i::Isolate* i_isolate() const {
    return reinterpret_cast<i::Isolate*>(isolate_);
  }

  v8::Local<v8::Context> context() const { return context_.Get(isolate_); }

  static StoreImpl* get(i::Isolate* isolate) {
    return static_cast<StoreImpl*>(
        reinterpret_cast<v8::Isolate*>(isolate)->GetData(0));
  }

 private:
  friend own<Store*> Store::make(Engine*);

  StoreImpl() {}

  v8::Isolate::CreateParams create_params_;
  v8::Isolate* isolate_ = nullptr;
  v8::Eternal<v8::Context> context_;
};

}  // namespace wasm

#endif  // V8_WASM_C_API_H_
