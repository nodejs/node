// Copyright (C) 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
features: [set-methods]
includes: [compareArray.js]
---*/

const firstSet = new Set();
firstSet.add(42);
firstSet.add(43);

const other = new Map();
other.set(42);
other.set(46);
other.set(47);

const resultSet = new Set();
resultSet.add(42);

const resultArray = Array.from(resultSet);
const intersectionArray = Array.from(firstSet.intersection(other));

assert.compareArray(resultArray, intersectionArray);
