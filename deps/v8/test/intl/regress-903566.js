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
// ICU uses bubblesort, so keep the array reasonably small (as of mid-2019:
// 100 entries -> 1ms, 1,000 entries -> 64ms, 10,000 entries -> 5s).
for (var i = 0; i < 100; i++) {
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
