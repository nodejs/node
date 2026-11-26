// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const big = 2n ** 64n;
const negative = -(2n ** 64n);
const longer = 2n ** 128n;
const otherDigit = 2n ** 65n;

const elements = [1, , 'x', negative, longer, otherDigit, big];

assertTrue(elements.includes(2n ** 64n));
assertEquals(6, elements.indexOf(2n ** 64n));
assertFalse(elements.includes(2n ** 66n));
assertEquals(-1, elements.indexOf(2n ** 66n));

assertTrue(Object.is(big, 2n ** 64n));
assertFalse(Object.is(big, negative));
assertFalse(Object.is(big, longer));
assertFalse(Object.is(big, otherDigit));

const set = new Set([otherDigit]);
assertFalse(set.has(big));
set.add(big);
assertTrue(set.has(2n ** 64n));

const map = new Map([[otherDigit, 1]]);
assertFalse(map.has(big));
map.set(big, 2);
assertEquals(2, map.get(2n ** 64n));
