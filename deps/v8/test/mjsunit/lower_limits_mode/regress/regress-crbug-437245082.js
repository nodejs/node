// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test: adding indexed properties to a dictionary-elements object
// should throw RangeError when the NumberDictionary capacity limit is
// exceeded, not crash with a fatal "invalid table size" OOM.

function TestDictionaryElementsOverflow() {
  // Force dictionary elements mode via a non-configurable property.
  let obj = {};
  Object.defineProperty(obj, 0, {value: 0, configurable: false});
  assertTrue(%HasDictionaryElements(obj));

  for (let i = 1; ; i++) {
    obj[i] = i;
  }
}

assertThrows(TestDictionaryElementsOverflow, RangeError,
             /Invalid array length/);
