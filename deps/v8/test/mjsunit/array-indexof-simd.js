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
