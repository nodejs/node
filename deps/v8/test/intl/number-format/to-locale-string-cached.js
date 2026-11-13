// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Number.prototype.toLocaleString and BigInt.prototype.toLocaleString use a
// cached formatter when the options argument is undefined. ICU switches the
// formatter to an optimized internal formatter after a few calls, so repeat
// each call and check that the result does not change.

const kRepeat = 10;

function check(locales, value) {
  const expected = new Intl.NumberFormat(locales).format(value);
  for (let i = 0; i < kRepeat; i++) {
    assertEquals(expected, value.toLocaleString(locales));
  }
}

const values = [
  0, -0, 1, -1, 1.5, 1234.5, -1234.5678, 0.1 + 0.2, 2 ** 53, 1e21, 1e-7,
  NaN, Infinity, -Infinity, 0n, -1n, 123456789012345678901234567890n,
];

for (const locales of [undefined, 'en', 'de', 'ja', 'ar']) {
  for (const value of values) check(locales, value);
}

// Alternate between locales, which replaces the cached formatter.
for (let i = 0; i < kRepeat; i++) {
  assertEquals('1,234.5', (1234.5).toLocaleString('en'));
  assertEquals('1.234,5', (1234.5).toLocaleString('de'));
  assertEquals('-1,234', (-1234n).toLocaleString('en'));
}

// Array.prototype.toLocaleString calls Number.prototype.toLocaleString for
// each element.
const array = [1234.5, -0.25, 6789, 1e21];
const expected_array =
    array.map(v => new Intl.NumberFormat('en').format(v)).join(',');
for (let i = 0; i < kRepeat; i++) {
  assertEquals(expected_array, array.toLocaleString('en'));
}
