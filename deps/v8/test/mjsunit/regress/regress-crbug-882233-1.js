// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intended to test bug [882233] on CSA fast-path.

let array = [];
Object.defineProperty(array, 'length', {writable: false});

assertEquals(array.length, 0);
assertThrows(() => array.shift(), TypeError);

let object = { length: 0 };
Object.defineProperty(object, 'length', {writable: false});

assertEquals(object.length, 0);
assertThrows(() => Array.prototype.shift.call(object));
