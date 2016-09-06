// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-explicit-tailcalls
"use strict";

function f() {
  return 1;
}

function g() {
  for (var v of [1, 2, 3]) {
    return continue f()  ;
  }
}
