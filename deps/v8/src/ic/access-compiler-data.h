// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_ACCESS_COMPILER_DATA_H_
#define V8_IC_ACCESS_COMPILER_DATA_H_

#include <memory>

#include "src/allocation.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class AccessCompilerData {
 public:
  AccessCompilerData() {}

  bool IsInitialized() const { return load_calling_convention_ != nullptr; }
  void Initialize(int load_register_count, const Register* load_registers,
                  int store_register_count, const Register* store_registers) {
    load_calling_convention_.reset(NewArray<Register>(load_register_count));
    for (int i = 0; i < load_register_count; ++i) {
      load_calling_convention_[i] = load_registers[i];
    }
    store_calling_convention_.reset(NewArray<Register>(store_register_count));
    for (int i = 0; i < store_register_count; ++i) {
      store_calling_convention_[i] = store_registers[i];
    }
  }

  Register* load_calling_convention() { return load_calling_convention_.get(); }
  Register* store_calling_convention() {
    return store_calling_convention_.get();
  }

 private:
  std::unique_ptr<Register[]> load_calling_convention_;
  std::unique_ptr<Register[]> store_calling_convention_;

  DISALLOW_COPY_AND_ASSIGN(AccessCompilerData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_ACCESS_COMPILER_DATA_H_
