// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pr = new Intl.PluralRules();
const inputs = [undefined, null, true, false, 1, '', 'test', {}, { a: 1 }];

inputs.forEach(input => {
  const number = Number(input);
  const expected = pr.select(number);
  const actual = pr.select(input);
  assertEquals(actual, expected);
});

let count = 0;
const dummyObject = {};
dummyObject[Symbol.toPrimitive] = () => ++count;
assertEquals(pr.select(dummyObject), pr.select(count));
assertEquals(count, 1);

assertEquals(pr.select(0), pr.select(-0))
