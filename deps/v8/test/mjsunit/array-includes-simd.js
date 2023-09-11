// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Large array of packed Smi, and Smi search_element
(() => {
  let a = [];
  for (let i = 0; i < 200; i++) {
    a[i] = i;
  }
  function testArrayIncludes(idx) {
    return a.includes(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    assertEquals(true, testArrayIncludes(i));
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i+from_index < 200; i += 2, from_index++) {
    assertEquals(true, testArrayIncludes(i, from_index));
  }
})();

// Large array of holey Smi, and Smi search_element
(() => {
  let a = [];
  // Skipping every other item when initializing
  for (let i = 0; i < 200; i+=2) {
    a[i] = i;
  }
  function testArrayIncludes(idx) {
    return a.includes(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    if (i % 2 == 0) {
      assertEquals(true, testArrayIncludes(i));
    } else {
      assertEquals(false, testArrayIncludes(i));
    }
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i + from_index < 200; i += 2, from_index++) {
    if (i % 2 == 0) {
      assertEquals(true, testArrayIncludes(i, from_index));
    } else {
      assertEquals(false, testArrayIncludes(i, from_index));
    }
  }
})();


// Large array of packed Doubles, and Double search_element
(() => {
  let a = [];
  for (let i = 0; i < 200; i++) {
    a[i] = i + 0.5;
  }
  function testArrayIncludes(idx) {
    return a.includes(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    assertEquals(true, testArrayIncludes(i + 0.5));
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i+from_index < 200; i += 2, from_index++) {
    assertEquals(true, testArrayIncludes(i+0.5, from_index));
  }
})();

// Large array of holey Doubles, and Double search_element
(() => {
  let a = [];
  // Skipping every other item when initializing
  for (let i = 0; i < 200; i+=2) {
    a[i] = i + 0.5;
  }
  function testArrayIncludes(idx) {
    return a.includes(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    if (i % 2 == 0) {
      assertEquals(true, testArrayIncludes(i + 0.5));
    } else {
      assertEquals(false, testArrayIncludes(i + 0.5));
    }
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i + from_index < 200; i += 2, from_index++) {
    if (i % 2 == 0) {
      assertEquals(true, testArrayIncludes(i+0.5, from_index));
    } else {
      assertEquals(false, testArrayIncludes(i+0.5, from_index));
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
  function testArrayIncludes(idx) {
    return a.includes(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    assertEquals(true, testArrayIncludes(b[i]));
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i+from_index < 200; i += 2, from_index++) {
    assertEquals(true, testArrayIncludes(b[i], from_index));
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
  function testArrayIncludes(idx) {
    return a.includes(idx);
  }
  // Without fromIndex
  for (let i = 0; i < 200; i++) {
    if (i % 2 == 0) {
      assertEquals(true, testArrayIncludes(b[i]));
    } else {
      assertEquals(false, testArrayIncludes(b[i]));
    }
  }
  // With fromIndex
  for (let i = 0, from_index = 0; i + from_index < 200; i += 2, from_index++) {
    if (i % 2 == 0) {
      assertEquals(true, testArrayIncludes(b[i], from_index));
    } else {
      assertEquals(false, testArrayIncludes(b[i], from_index));
    }
  }
})();
