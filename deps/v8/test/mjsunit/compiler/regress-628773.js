// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-tailcalls

"use strict";

function foo() {
  for  (var i = 0; i < 10000; i++) {
    try {
      for (var j = 0; j < 2; j++) {
      }
      throw 1;
    } catch(e) {
      if (typeof a == "number") return a && isNaN(b);
    }
  }
}

foo();
