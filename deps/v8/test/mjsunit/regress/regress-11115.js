// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stack-size=100

var f = function() {}

for (var i = 0; i < 1000; ++i) {
  f = f.bind();
  Object.defineProperty(f, Symbol.hasInstance, {value: undefined});
}

try {
  ({}) instanceof f;  // Don't overflow the stack!
} catch (e) {
  // Throwing a RangeError is okay.
}
