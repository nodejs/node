// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
    for (let i = 0; i < 50; i++) {
      if (i == 35) {
        i *= 35;
      }
    }
}
[ 0.0, 936.657341697756, 4.0].forEach(foo);
