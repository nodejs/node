// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMPILATION_DEPENDENCY_H_
#define V8_COMPILER_COMPILATION_DEPENDENCY_H_

#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class MaybeObjectHandle;

namespace compiler {

class CompilationDependency : public ZoneObject {
 public:
  virtual bool IsValid() const = 0;
  virtual void PrepareInstall() const {}
  virtual void Install(const MaybeObjectHandle& code) const = 0;

#ifdef DEBUG
  virtual bool IsPretenureModeDependency() const { return false; }
  virtual bool IsFieldRepresentationDependencyOnMap(
      Handle<Map> const& receiver_map) const {
    return false;
  }
#endif
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMPILATION_DEPENDENCY_H_
