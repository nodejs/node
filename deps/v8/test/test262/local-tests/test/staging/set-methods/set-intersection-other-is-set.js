// Copyright (C) 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
features: [set-methods]
includes: [compareArray.js]
---*/

const firstSet = new Set();
firstSet.add(42);
firstSet.add(43);
firstSet.add(44);

const otherSet = new Set();
otherSet.add(42);
otherSet.add(45);

const resultSet = new Set();
resultSet.add(42);

const resultArray = Array.from(resultSet);
const intersectionArray = Array.from(firstSet.intersection(otherSet));

assert.compareArray(resultArray, intersectionArray);
