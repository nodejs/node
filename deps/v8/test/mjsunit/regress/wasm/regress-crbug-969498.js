// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let global = new WebAssembly.Global({value: 'f32', mutable: true}, 2e66);
global.value = 2e66;

// Custom additional test case:
const kRoundsDown = 3.4028235677973362e+38;
const kRoundsToInf = 3.4028235677973366e+38;
var floats = new Float32Array([kRoundsDown, kRoundsToInf]);
assertNotEquals(Infinity, floats[0]);
assertEquals(Infinity, floats[1]);
floats.set([kRoundsDown, kRoundsToInf]);
assertNotEquals(Infinity, floats[0]);
assertEquals(Infinity, floats[1]);
