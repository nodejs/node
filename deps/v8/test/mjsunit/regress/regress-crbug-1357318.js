// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function array_iterator() {
  let count = 0;
  [].values().__proto__.return = function(value) {
    ++count;
    return {value: value, done: true};
  };

  let array = [1, 2, 3, 4, 5, 6, 7, 8];

  // Aborted iteration in a builtin.
  try {
    new WeakSet(array);
  } catch (e) {}
  assertEquals(count, 1);

  // Aborted iteration via for..of.
  let i = array.length / 2;
  for (c of array) {
    if (--i == 0) break;
  }
  assertEquals(count, 2);
})();

(function set_iterator() {
  let count = 0;
  new Set().values().__proto__.return = function(value) {
    ++count;
    return {value: value, done: true};
  };

  let set = new Set();
  for (let i = 0; i < 26; i++) {
    set.add("item" + i);
  }

  // Aborted iteration in a builtin.
  try {
    new WeakSet(set);
  } catch (e) {}
  assertEquals(count, 1);

  // Aborted iteration via for..of.
  let i = set.size / 2;
  for (c of set.values()) {
    if (--i == 0) break;
  }
  assertEquals(count, 2);
})();

(function map_iterator() {
  let count = 0;
  new Map().values().__proto__.return = function(value) {
    ++count;
    return {value: value, done: true};
  };

  let map = new Map();
  for (let i = 0; i < 26; i++) {
    map.set(String.fromCharCode(97 + i), i);
  }

  // Aborted iteration in a builtin.
  try {
    new WeakMap(map);
  } catch (e) {}
  assertEquals(count, 1);

  // Aborted iteration via for..of.
  let i = map.size / 2;
  for (c of map.keys()) {
    if (--i == 0) break;
  }
  assertEquals(count, 2);
})();

(function string_iterator() {
  let count = 0;
  let str = "some long string";
  let iterator = str[Symbol.iterator]();
  iterator.__proto__.return = function(value) {
    ++count;
    return {value: value, done: true};
  };

  // Aborted iteration in a builtin.
  try {
    new WeakSet(iterator);
  } catch (e) {}
  assertEquals(count, 1);

  // Aborted iteration via for..of.
  let i = str.length / 2;
  for (c of iterator) {
    if (--i == 0) break;
  }
  assertEquals(count, 2);
})();
