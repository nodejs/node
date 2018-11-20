// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_H_
#define V8_CODE_H_

#include "src/allocation.h"
#include "src/handles.h"
#include "src/objects.h"

namespace v8 {
namespace internal {


// Wrapper class for passing expected and actual parameter counts as
// either registers or immediate values. Used to make sure that the
// caller provides exactly the expected number of parameters to the
// callee.
class ParameterCount BASE_EMBEDDED {
 public:
  explicit ParameterCount(Register reg)
      : reg_(reg), immediate_(0) { }
  explicit ParameterCount(int immediate)
      : reg_(no_reg), immediate_(immediate) { }
  explicit ParameterCount(Handle<JSFunction> f)
      : reg_(no_reg),
        immediate_(f->shared()->internal_formal_parameter_count()) {}

  bool is_reg() const { return !reg_.is(no_reg); }
  bool is_immediate() const { return !is_reg(); }

  Register reg() const {
    DCHECK(is_reg());
    return reg_;
  }
  int immediate() const {
    DCHECK(is_immediate());
    return immediate_;
  }

 private:
  const Register reg_;
  const int immediate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ParameterCount);
};


} }  // namespace v8::internal

#endif  // V8_CODE_H_
