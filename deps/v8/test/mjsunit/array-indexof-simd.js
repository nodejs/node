// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Large array of packed Smi, and Smi search_element
(() => {
  let a = [];
  for (let i = 0; i < 200; i++) {
    a[i] = i;
  }
  function testArrayIndexOf(idx) {
    return a.indexOf(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    assertEquals(i, testArrayIndexOf(i));
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i+from_index < 200; i += 2, from_index++) {
    assertEquals(i, testArrayIndexOf(i, from_index));
  }
})();

// Large array of holey Smi, and Smi search_element
(() => {
  let a = [];
  // Skipping every other item when initializing
  for (let i = 0; i < 200; i+=2) {
    a[i] = i;
  }
  function testArrayIndexOf(idx) {
    return a.indexOf(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    if (i % 2 == 0) {
      assertEquals(i, testArrayIndexOf(i));
    } else {
      assertEquals(-1, testArrayIndexOf(i));
    }
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i + from_index < 200; i += 2, from_index++) {
    if (i % 2 == 0) {
      assertEquals(i, testArrayIndexOf(i, from_index));
    } else {
      assertEquals(-1, testArrayIndexOf(i, from_index));
    }
  }
})();

// Large array of packed Doubles, and Double search_element
(() => {
  let a = [];
  for (let i = 0; i < 200; i++) {
    a[i] = i + 0.5;
  }
  function testArrayIndexOf(idx) {
    return a.indexOf(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    assertEquals(i, testArrayIndexOf(i + 0.5));
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i+from_index < 200; i += 2, from_index++) {
    assertEquals(i, testArrayIndexOf(i+0.5, from_index));
  }
})();

// Large array of holey Doubles, and Double search_element
(() => {
  let a = [];
  // Skipping every other item when initializing
  for (let i = 0; i < 200; i+=2) {
    a[i] = i + 0.5;
  }
  function testArrayIndexOf(idx) {
    return a.indexOf(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    if (i % 2 == 0) {
      assertEquals(i, testArrayIndexOf(i + 0.5));
    } else {
      assertEquals(-1, testArrayIndexOf(i + 0.5));
    }
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i + from_index < 200; i += 2, from_index++) {
    if (i % 2 == 0) {
      assertEquals(i, testArrayIndexOf(i+0.5, from_index));
    } else {
      assertEquals(-1, testArrayIndexOf(i+0.5, from_index));
    }
  }
})();

// Large array of packed objects, and object search_element
(() => {
  let a = [];
  let b = [];
  for (let i = 0; i < 200; i++) {
    a[i] = { v: i };
    b[i] = a[i];
  }
  function testArrayIndexOf(idx) {
    return a.indexOf(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    assertEquals(i, testArrayIndexOf(b[i]));
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i+from_index < 200; i += 2, from_index++) {
    assertEquals(i, testArrayIndexOf(b[i], from_index));
  }
})();

// Large array of holey objects, and object search_element
(() => {
  let a = [];
  let b = [];
  // Skipping every other item when initializing
  for (let i = 0; i < 200; i++) {
    b[i] = { v: i };
    if (i % 2 == 0) {
      a[i] = b[i];
    }
  }
  function testArrayIndexOf(idx) {
    return a.indexOf(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    if (i % 2 == 0) {
      assertEquals(i, testArrayIndexOf(b[i]));
    } else {
      assertEquals(-1, testArrayIndexOf(b[i]));
    }
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i + from_index < 200; i += 2, from_index++) {
    if (i % 2 == 0) {
      assertEquals(i, testArrayIndexOf(b[i], from_index));
    } else {
      assertEquals(-1, testArrayIndexOf(b[i], from_index));
    }
  }
})();

// This test checks that when the item that IndexOf searches is present multiple
// time, the correct index is returned (in particular, when a single SIMD vector
// had multiple matches). For instance, if we do:
//
//   [1, 2, 1, 3].indexOf(1)
//
// Then it should return 0 rather than 2.
(() => {


  // The patterns that this function will check, where for instance patternABAB
  // means that we'd like to build a vector containing {A, B, A, B}.
  let patterns = {
    patternABAB : (a, b, c, d) => [a, b, a, b, c, d, c, d],
    patternAABB : (a, b, c, d) => [a, a, b, b, c, c, d, d],
    patternABBA : (a, b, c, d) => [a, b, b, a, c, d, d, c],
    patternABAA : (a, b, c, d) => [a, b, a, a, c, d, c, c],
    patternAABA : (a, b, c, d) => [a, a, b, a, c, c, d, c],
    patternAAAB : (a, b, c, d) => [a, a, a, b, c, c, c, d],
    patternBAAA : (a, b, c, d) => [b, a, a, a, d, c, c, c]
  };

  // Starting |a| with a bunch of 0s, which might be handled by the scalar loop
  // that the SIMD code does to reach 16/32-byte alignment.
  let a = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
  let next_int = 1;
  for (const [_, pattern] of Object.entries(patterns)) {
    // It's a bit tricky to ensure that 2 items will be in the same SIMD batch
    // because we can't control the alignment of the array from JS, and the
    // SIMD code will start by skipping the first items to have the memory
    // aligned on 16/32 bytes. So, we put each pattern 8 times in a row in |a|,
    // but each time with an additional item, to make sure that each of those 8
    // repeated pattern have a different alignment.
    for (let i = 0; i < 8; i++) {
      a = a.concat(pattern(next_int, next_int + 1, next_int + 2, next_int + 3));
      a.push(next_int + 4);  // By adding a 9th item, we make sure that the
                             // alignment of the next batch is not the same as
                             // the current one.
      next_int += 5;
    }
  }

  let b = a.slice();
  b[10000] = 42;  // Switch b to dictionary mode so that the SIMD code won't be
                  // used for it. We can then use `b.indexOf` as reference.

  for (let x of b) {
    if (x == undefined) break;
    assertEquals(b.indexOf(x), a.indexOf(x));
  }
})();

// Match at every position and no match, for lengths covering every remainder
// of the 4x-unrolled loop, the 1x loop and the scalar tail. Arrays shorter than
// 48 elements do not take the SIMD path. Objects share the Smi code path.
(() => {
  const sentinelSmi = 99999;
  const sentinelDouble = 99999.5;

  for (let len = 48; len < 68; len++) {
    const smiArr = new Array(len);
    const dblArr = new Array(len);
    for (let i = 0; i < len; i++) {
      smiArr[i] = i;
      dblArr[i] = i + 0.5;
    }

    assertEquals(-1, smiArr.indexOf(sentinelSmi));
    assertEquals(-1, dblArr.indexOf(sentinelDouble));

    for (let pos = 0; pos < len; pos++) {
      assertEquals(pos, smiArr.indexOf(pos));
      assertEquals(pos, dblArr.indexOf(pos + 0.5));
    }
  }

  // Two matches at varying distances, in the same and in different vectors of
  // an unrolled iteration (16 Smi or 8 Double elements).
  const len = 80;
  const baseSmi = new Array(len).fill(0);
  const baseDbl = new Array(len).fill(0.5);
  for (let first = 0; first < 48; first += 3) {
    for (let delta of [1, 3, 4, 5, 7, 8, 12, 15, 16]) {
      const second = first + delta;
      baseSmi[first] = 42;
      baseSmi[second] = 42;
      baseDbl[first] = 42.5;
      baseDbl[second] = 42.5;
      assertEquals(first, baseSmi.indexOf(42));
      assertEquals(first, baseDbl.indexOf(42.5));
      assertEquals(second, baseSmi.indexOf(42, first + 1));
      assertEquals(second, baseDbl.indexOf(42.5, first + 1));
      baseSmi[first] = 0;
      baseSmi[second] = 0;
      baseDbl[first] = 0.5;
      baseDbl[second] = 0.5;
    }
  }
})();
