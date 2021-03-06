// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt

x = "";

function f () {
  function g() {
    try {
      eval('');
      return x;
    } catch(e) {
    }
  }
  return g();
}

f();
