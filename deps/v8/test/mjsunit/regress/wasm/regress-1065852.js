// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function* asm() {
  "use asm";
  function x(v) {
    v = v | 0;
  }
  return x;
}

// 'function*' creates a generator with an implicit 'next' method.
asm().next();
