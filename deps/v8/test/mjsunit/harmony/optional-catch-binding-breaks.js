// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-optional-catch-binding

let state = 'initial';
x: try {
  throw new Error('caught');
  state = 'unreachable';
} catch {
  assertEquals(state, 'initial');
  state = 'caught';
  break x;
  state = 'unreachable';
}
assertEquals(state, 'caught');


state = 'initial';
x: try {
  throw new Error('caught');
  state = 'unreachable';
} catch {
  assertEquals(state, 'initial');
  state = 'caught';
  break x;
  state = 'unreachable';
} finally {
  assertEquals(state, 'caught');
  state = 'finally';
}
assertEquals(state, 'finally');


state = 'initial';
x: {
  y: try {
    throw new Error('caught');
    state = 'unreachable';
  } catch {
    assertEquals(state, 'initial');
    state = 'caught';
    break x;
    state = 'unreachable';
  } finally {
    assertEquals(state, 'caught');
    state = 'finally';
    break y;
    state = 'unreachable';
  }
  assertEquals(state, 'finally');
  state = 'after block';
}
assertEquals(state, 'after block');


do {
  try {
    throw new Error();
  } catch {
    break;
  }
  assertUnreachable();
} while(false);
