// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/bytecode-liveness-map.h"

namespace v8 {
namespace internal {
namespace compiler {

std::string ToString(const BytecodeLivenessState& liveness) {
  std::string out;
  out.resize(liveness.register_count() + 1);
  for (int i = 0; i < liveness.register_count(); ++i) {
    if (liveness.RegisterIsLive(i)) {
      out[i] = 'L';
    } else {
      out[i] = '.';
    }
  }
  if (liveness.AccumulatorIsLive()) {
    out[liveness.register_count()] = 'L';
  } else {
    out[liveness.register_count()] = '.';
  }
  return out;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
