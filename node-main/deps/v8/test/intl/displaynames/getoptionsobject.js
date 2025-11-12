// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test Intl.DisplayNames call GetOptionsObject instead of ToObject
// https://tc39.es/ecma402/#sec-getoptionsobject
// https://tc39.es/ecma262/#sec-toobject
let testCases = [
    null, // Null
    true, // Boolean
    false, // Boolean
    1234, // Number
    "string", // String
    Symbol('foo'),  // Symbol
    9007199254740991n // BigInt
];

testCases.forEach(function (testCase) {
  assertThrows(() => new Intl.DisplayNames("en", testCase), TypeError);
});
