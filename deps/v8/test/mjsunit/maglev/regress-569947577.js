// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --maglev-osr --no-turbofan --no-concurrent-osr

function foo(n) {
  let outer_var = 0;
  {
    let block_var = 1;
    for (let i = 0; i < n; i++) {
      let inner = function() {
        outer_var += 3;
        return block_var;
      };
      outer_var = outer_var + 1;
      inner();
      outer_var = outer_var + 1;
    }
    return outer_var;
  }
}

assertEquals(5000, foo(1000));
