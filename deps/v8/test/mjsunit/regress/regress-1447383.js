// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Correctness issue:
// When changing the Number prototype and providing a smi as the locale for
// String.p.toLocaleLowerCase, the prototype would not be considered.
// See ecma402 #sec-canonicalizelocalelist.

let smi = 123;
let heapNumber = 1.23;

// Test case 1: The number prototype has a length property and a valid locale
// for Get(O, '0').
Number.prototype.__proto__ = ['tr'];
assertEquals('I'.toLocaleLowerCase('tr'),
             'I'.toLocaleLowerCase(smi));
assertEquals('I'.toLocaleLowerCase('tr'),
             'I'.toLocaleLowerCase(heapNumber));

// Test case 2: The number prototype has a length property and an invalid locale
// for Get(O, '0').
Number.prototype.__proto__ = [42];  // 42 is not a valid locale.
assertThrows(() => 'I'.toLocaleLowerCase(smi), TypeError);
assertThrows(() => 'I'.toLocaleLowerCase(heapNumber), TypeError);

// Test case 3: The number prototype has a length property of 0.
Number.prototype.__proto__ = [];
assertEquals('I'.toLocaleLowerCase([]),
             'I'.toLocaleLowerCase(smi));
assertEquals('I'.toLocaleLowerCase([]),
             'I'.toLocaleLowerCase(heapNumber));
