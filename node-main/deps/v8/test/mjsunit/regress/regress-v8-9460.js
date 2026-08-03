// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var arr = [0, 1];

assertThrows(
  () => Object.defineProperty(arr, 'length', {value: 1, configurable: true}),
  TypeError);
assertEquals(2, arr.length);

assertThrows(
  () => Object.defineProperty(arr, 'length', {value: 2, configurable: true}),
  TypeError);
assertEquals(2, arr.length);

assertThrows(
  () => Object.defineProperty(arr, 'length', {value: 3, configurable: true}),
  TypeError);
assertEquals(2, arr.length);
