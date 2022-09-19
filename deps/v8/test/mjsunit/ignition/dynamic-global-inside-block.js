// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Verifies that DYNAMIC_GLOBAL variables walk the correct context-chain length
// to reach the sloppy-eval calling function context, including block contexts.
function test() {
  return eval('var x = 100; { function z() {z}; x }')
}

test();
