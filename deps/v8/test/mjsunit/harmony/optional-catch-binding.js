// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-optional-catch-binding

let state = 'initial';
try {
  throw new Error('caught');
  state = 'unreachable';
} catch { // Note the lack of a binding
  assertEquals(state, 'initial');
  state = 'caught';
}
assertEquals(state, 'caught');


let sigil1 = {};
try {
  throw sigil1;
} catch (e) {
  assertEquals(e, sigil1);
}


let sigil2 = {};
let reached = false;
try {
  try {
    throw sigil1;
  } catch {
    reached = true;
  } finally {
    throw sigil2;
  }
} catch (e) {
  assertEquals(e, sigil2);
}
assertTrue(reached);
