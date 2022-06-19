// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function print_stack(unsigned) {
  print('stack:');
  print((new Error()).stack);
}

function asm(global, env) {
  'use asm';

  var print_stack = env.print_stack;

  function main() {
    var count = 0;

    while ((count | 0) < 10) {
      print_stack(1);
      count = count + 1 | 0;
    }
  }

  return main;
}

asm({}, {'print_stack': print_stack})();
