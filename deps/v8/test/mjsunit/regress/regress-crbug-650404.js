// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function c4(w, h) {
  var size = w * h;
  if (size < 0) size = 0;
  return new Uint32Array(size);
}

for (var i = 0; i < 3; i++) {
  // Computing -0 as the result makes the "size = w * h" multiplication IC
  // go into double mode.
  c4(0, -1);
}
// Optimize Uint32ConstructFromLength.
for (var i = 0; i < 1000; i++) c4(2, 2);

// This array will have a HeapNumber as its length:
var bomb = c4(2, 2);

function reader(o, i) {
  // Dummy try-catch, so that TurboFan is used to optimize this.
  try {} catch(e) {}
  return o[i];
}
// Optimize reader!
for (var i = 0; i < 3; i++) reader(bomb, 0);
%OptimizeFunctionOnNextCall(reader);
reader(bomb, 0);

for (var i = bomb.length; i < 100; i++) {
  assertEquals(undefined, reader(bomb, i));
}
