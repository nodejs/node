// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags:  --allow-natives-syntax

assertDoesNotThrow(()=>(new Intl.ListFormat()).format());
// Intl.getCanonicalLocales() will create a HOLEY_ELEMENTS array
assertDoesNotThrow(()=>(new Intl.ListFormat()).format(Intl.getCanonicalLocales()));
assertDoesNotThrow(()=>(new Intl.ListFormat()).format(Intl.getCanonicalLocales(["en","fr"])));

let arr = ["a","b","c"];

// Test under no HasHoleyElements();
assertFalse(%HasHoleyElements(arr));
assertDoesNotThrow(()=>(new Intl.ListFormat()).format(arr));
for (var i = 0; i < 10000; i++) {
  arr.push("xx");
}
assertFalse(%HasHoleyElements(arr));
assertDoesNotThrow(()=>(new Intl.ListFormat()).format(arr));

// Test under HasHoleyElements();
arr[arr.length + 10] = "x";
assertTrue(%HasHoleyElements(arr));
assertFalse(%HasDictionaryElements(arr));
assertThrows(()=>(new Intl.ListFormat()).format(arr), TypeError);

// Test it work under HasDictionaryElements();
arr = ["a","b","c"];
arr[arr.length + 100000] = "x";
assertTrue(%HasDictionaryElements(arr));
assertThrows(()=>(new Intl.ListFormat()).format(arr), TypeError);
