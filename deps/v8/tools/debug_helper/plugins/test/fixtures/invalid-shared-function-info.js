// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function test_func_3() {
  corruptSFIAndAbort(test_func_3);
}

function test_func_2() {
  return test_func_3();
}

function test_func_1() {
  return test_func_2();
}

test_func_1();
