// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_STATIC_ROOTS_GEN_H_
#define V8_SNAPSHOT_STATIC_ROOTS_GEN_H_

namespace v8 {
namespace internal {

class Isolate;

class StaticRootsTableGen {
 public:
  static void write(Isolate* isolate, const char* file);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_STATIC_ROOTS_GEN_H_
