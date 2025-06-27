// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan --function-context-specialization --verify-heap

function* gen() {
  for (var i = 0; i < 3; ++i) {
    yield i
  }
}
gen().next();
