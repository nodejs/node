// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var locales = ['en-US', 'fr'];
var result = Intl.getCanonicalLocales(locales);
var len = result.length

// TODO(jshin): Remove the following when
// https://github.com/tc39/test262/issues/745 is resolved and
// test262 in v8 is updated.

assertEquals(Object.getPrototypeOf(result), Array.prototype);
assertEquals(result.constructor, Array);

for (var key in result) {
  var desc = Object.getOwnPropertyDescriptor(result, key);
  assertTrue(desc.writable);
  assertTrue(desc.configurable);
  assertTrue(desc.enumerable);
}

var desc = Object.getOwnPropertyDescriptor(result, 'length');
assertTrue(desc.writable);
assertEquals(result.push('de'), desc.value + 1);
