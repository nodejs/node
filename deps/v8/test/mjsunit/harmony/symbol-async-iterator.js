// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-iteration

assertTrue(Symbol.hasOwnProperty('asyncIterator'));
assertEquals('symbol', typeof Symbol.asyncIterator);
assertInstanceof(Object(Symbol.asyncIterator), Symbol);

let desc = Object.getOwnPropertyDescriptor(Symbol, 'asyncIterator');
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);
