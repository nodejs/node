// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  'use asm';
  function bar() {
    return -1e-15;
  }
  return {bar: bar};
}

assertEquals(-1e-15, foo().bar());
