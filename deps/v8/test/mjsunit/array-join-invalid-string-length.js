// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function DictionaryStringRepeatFastPath() {
  const a = new Array(%StringMaxLength());
  assertTrue(%HasDictionaryElements(a));
  const sep = '12';
  assertThrows(() => a.join(sep), RangeError);

  // Verifies cycle detection still works properly after thrown error.
  assertThrows(() => a.join(sep), RangeError);

  a.length = 3;
  a[0] = 'a';
  a[1] = 'b';
  a[2] = 'c';
  assertSame('a,b,c', a.join());
})();

(function SeparatorOverflow() {
  const a = ['a',,,,,'b'];

  const sep = ','.repeat(%StringMaxLength());
  assertThrows(() => a.join(sep), RangeError);

  // Verifies cycle detection still works properly after thrown error.
  assertThrows(() => a.join(sep), RangeError);
  assertSame('a,,,,,b', a.join());
})();

(function ElementOverflow() {
  const el = ','.repeat(%StringMaxLength());
  const a = [el, el, el, el, el];

  assertThrows(() => a.join(), RangeError);

  // Verifies cycle detection still works properly after thrown error.
  assertThrows(() => a.join(), RangeError);
  a[0] = 'a';
  a[1] = 'b';
  a[2] = 'c';
  a[3] = 'd';
  a[4] = 'e';
  assertSame('a,b,c,d,e', a.join());
})();

(function ElementSeparatorOverflow() {
  const el = ','.repeat(%StringMaxLength());
  const a = [el, el, el, el];

  assertThrows(() => a.join(el), RangeError);

  // Verifies cycle detection still works properly after thrown error.
  assertThrows(() => a.join(el), RangeError);
  a[0] = 'a';
  a[1] = 'b';
  a[2] = 'c';
  a[3] = 'd';
  assertSame('a,b,c,d', a.join());
})();
