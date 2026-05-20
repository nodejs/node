// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* f() {
  x.__defineGetter__();
  var x = 0;
  for (let y of iterable) {
    yield y;
  }
}

f();
