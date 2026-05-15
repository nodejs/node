// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: memory_corruption/input.js
foo();
bar();
let __v_0 = {};
foo();
corrupt(__v_0, 3388261501);
function __f_1() {
  foo();
  const __v_1 = 0;
  const __v_2 = 1;
  const __v_3 = 2;
  const __v_4 = 3;
  foo();
  corrupt(__v_3, 3418614645);
  bar();
  corrupt(__v_4, 2678112981);
  corrupt(__v_4, 2839651430);
  baz();
  corrupt(__v_1, 4096809706);
  corrupt(__v_1, 2657152084);
  if (true) {
    bar();
    corrupt(__v_0, 2033548496);
    baz();
    corrupt(__v_0, 3531751247);
    corrupt(__v_0, 1632394196);
  }
}
foo();
corrupt(__v_0, 3106090743);
corrupt(__v_0, 2097918696);
bar();
corrupt(__v_0, 788753159);
corrupt(__v_0, 1204442518);
baz();
corrupt(__v_0, 487310280);
