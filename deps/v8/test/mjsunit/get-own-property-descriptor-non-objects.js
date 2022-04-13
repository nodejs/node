// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(function() {
  Object.getOwnPropertyDescriptor(null, 'x');
}, TypeError);


assertThrows(function() {
  Object.getOwnPropertyDescriptor(undefined, 'x');
}, TypeError);


assertEquals({
  configurable: false,
  enumerable: false,
  value: 3,
  writable: false,
}, Object.getOwnPropertyDescriptor('abc', 'length'));


assertEquals({
  configurable: false,
  enumerable: true,
  value: 'a',
  writable: false,
}, Object.getOwnPropertyDescriptor('abc', 0));


assertSame(undefined, Object.getOwnPropertyDescriptor(42, 'x'));
assertSame(undefined, Object.getOwnPropertyDescriptor(true, 'x'));
assertSame(undefined, Object.getOwnPropertyDescriptor(false, 'x'));
assertSame(undefined, Object.getOwnPropertyDescriptor(Symbol(), 'x'));
