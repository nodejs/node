// Copyright (C) 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
features: [set-methods]
includes: [compareArray.js]
---*/

const SetLike = {
    arr: [42, 43, 45],
    size: 3,
        keys() {
            return this.arr[Symbol.iterator]();
        },
        has(key) {
            return this.arr.indexOf(key) != -1;
        }
    };

const firstSet = new Set();
firstSet.add(42);
firstSet.add(43);

const resultSet = new Set();
resultSet.add(42);
resultSet.add(43);

const resultArray = Array.from(resultSet);
const intersectionArray = Array.from(firstSet.intersection(SetLike));

assert.compareArray(resultArray, intersectionArray);
