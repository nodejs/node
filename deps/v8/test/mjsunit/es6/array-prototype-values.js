// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --harmony-array-prototype-values

// Functionality of the values iterator is tested elsewhere; this test
// merely verifies that the 'values' property is set up correctly.
var valuesDesc = Object.getOwnPropertyDescriptor(Array.prototype, 'values');
assertEquals('object', typeof valuesDesc);
assertSame(Array.prototype[Symbol.iterator], valuesDesc.value);
assertTrue(valuesDesc.configurable);
assertTrue(valuesDesc.writable);
assertFalse(valuesDesc.enumerable);
assertTrue(Array.prototype[Symbol.unscopables].values);
assertThrows(() => new Array.prototype[Symbol.iterator], TypeError);
