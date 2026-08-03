// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test the locales in src/third_party/icu/source/data/locales/ which
// has %%ALIAS output the same as what it alias to instead of root.
const aliases = new Map([
  ['sh', 'sr-Latn'],
  ['in', 'id'],
  ['mo', 'ro'],
  ['iw', 'he'],
  ['no', 'nb'],
  ['tl', 'fil'],
  ['iw-IL', 'he-IL'],
  ['sr-CS', 'sr-Cyrl-RS'],
]);

const date = new Date();
const number = 123456789.123456789;
for (const [from, to] of aliases) {
  const fromDTF = new Intl.DateTimeFormat(from, {month: 'long', weekday: 'long'});
  const toDTF = new Intl.DateTimeFormat(to, {month: 'long', weekday: 'long'});
  for (let m = 0; m < 12; m++) {
    date.setMonth(m);
    assertEquals(fromDTF.format(date), toDTF.format(date),
        `Expected to see the same output from "${from}" and "${to}".`);
  }
  const fromNF = new Intl.NumberFormat(from);
  const toNF = new Intl.NumberFormat(to);
  assertEquals(fromNF.format(number), toNF.format(number),
      `Expected to see the same output from "${from}" and "${to}".`);
}
