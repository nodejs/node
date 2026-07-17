// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function print_stack(unused_arg) {
  console.trace();
}
function asm(_, imports) {
  'use asm';
  var print_stack = imports.print_stack;
  function f() {
      print_stack(1);
  }
  return f;
}
asm({}, {'print_stack': print_stack})();
