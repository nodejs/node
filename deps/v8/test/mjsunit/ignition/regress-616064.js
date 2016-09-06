// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition

function foo() {
  if (this.Worker) {
    function __f_0() { this.s = a; }
    function __f_1() {
      this.l = __f_0;
    }

    with ( 'source' , Object ) throw function __f_0(__f_0) {
      return Worker.__f_0(-2147483648, __f_0);
    };

    var __v_9 = new Worker('');
    __f_1 = {s: Math.s, __f_1: true};
  }
}

try {
  foo();
} catch(e) {
}
