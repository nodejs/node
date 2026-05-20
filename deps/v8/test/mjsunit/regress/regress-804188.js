// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Object.defineProperty(Array.prototype, Symbol.iterator, {
  value: function* () {}
});
const arrayIteratorProto = Object.getPrototypeOf([][Symbol.iterator]());
arrayIteratorProto.next = function() {};

assertThrows(() => new Map([[{}, 1], [{}, 2]]), TypeError);
assertThrows(() => new WeakMap([[{}, 1], [{}, 2]]), TypeError);
assertThrows(() => new Set([{}]), TypeError);
assertThrows(() => new WeakSet([{}]), TypeError);
