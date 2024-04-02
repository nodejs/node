// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-array-grouping

assertEquals(Array.prototype[Symbol.unscopables].groupByToMap, true);

var array = [-0, 1, 0, 2];
var groupByToMap = () => {
  let result = array.groupByToMap(v => v > 0);
  result = Array.from(result.entries());
  return result;
}

// entry order matters
assertEquals(groupByToMap(), [
  [false, [-0, 0]],
  [true, [1, 2]],
]);

Object.defineProperty(array, 4, {
  enumerable: true,
  configurable: true,
  writable: true,
  value: 3,
});
assertEquals(groupByToMap(), [
  [false, [-0, 0]],
  [true, [1, 2, 3]],
]);

Object.defineProperty(array, 5, {
  enumerable: true,
  configurable: true,
  get: () => 4,
});
var result = groupByToMap();
assertEquals(result, [
  [false, [-0, 0]],
  [true, [1, 2, 3, 4]],
]);
assertSame(result[0][1][0], -0);


// fairly large result array
var length = 20000;
var array = new Array(length);
for (var idx = 0; idx < length; idx++) {
  array[idx] = idx;
}
var groupByToMap = () => {
  let result = array.groupByToMap(v => v % 2);
  result = Array.from(result.entries());
  return result;
}
var result = groupByToMap();
assertEquals(result, [
  [0, array.filter(v => v % 2 === 0)],
  [1, array.filter(v => v % 2 === 1)],
]);


// check section groupByToMap 6.d
var array = [-0, 0];
var result = array.groupByToMap(v => v);
assertEquals(result.get(0), [-0, 0]);


// check array changed by callbackfn
var array = [-0, 0, 1, 2];
var groupByToMap = () => {
  let result = array.groupByToMap((v, idx) => {
    if (idx === 1) {
      array[2] = {a: 'b'};
    }
    return v > 0;
  });
  result = Array.from(result.entries());
  return result;
}

assertEquals(groupByToMap(), [
  [false, [-0, 0, {a: 'b'}]],
  [true, [2]],
]);

// check array with holes
var array = [1, , 2, , 3, , 4];
var groupByToMap = () => {
  let result = array.groupByToMap(v => v % 2 === 0 ? 'even' : 'not_even');
  result = Array.from(result.entries());
  return result;
};
function checkNoHoles(arr) {
  for (let idx = 0; idx < arr.length; idx++) {
    assertTrue(Object.getOwnPropertyDescriptor(arr, idx) !== undefined);
  }
}
var result = groupByToMap();
assertEquals(result, [
  ['not_even', [1, undefined, undefined, 3, undefined]],
  ['even', [2, 4]],
]);
checkNoHoles(result[0][1]);
checkNoHoles(result[1][1]);

var array = [1, undefined, 2, undefined, 3, undefined, 4];
result = groupByToMap();
assertEquals(result, [
  ['not_even', [1, undefined, undefined, 3, undefined]],
  ['even', [2, 4]],
]);
checkNoHoles(result[0][1]);
checkNoHoles(result[1][1]);

// array like objects
var arrayLikeObjects = [
  {
    '0': -1,
    '1': 1,
    '2': 2,
    length: 3,
  },
  (function () { return arguments })(-1, 1, 2),
  Int8Array.from([-1, 1, 2]),
  Float32Array.from([-1, 1, 2]),
];
var groupByToMap = () => {
  let result = Array.prototype.groupByToMap.call(array, v => v > 0);
  result = Array.from(result.entries());
  return result;
};
for (var array of arrayLikeObjects) {
  assertEquals(groupByToMap(), [
    [false, [-1]],
    [true, [1, 2]],
  ]);
}

// check proto elements
var array = [,];
var groupByToMap = () => {
  let result = array.groupByToMap(v => v);
  result = Array.from(result.entries());
  return result;
}

assertEquals(groupByToMap(), [
  [undefined, [,]],
]);
array.__proto__.push(6);
assertEquals(groupByToMap(), [
  [6, [6]],
]);


// callbackfn throws
var array = [-0, 1, 0, 2];
assertThrows(
  () => array.groupByToMap(() => { throw new Error('foobar'); }),
  Error,
  'foobar'
);


// callbackfn is not callable
var array = [-0, 1, 0, 2];
assertThrows(
  () => array.groupByToMap('foobar'),
  TypeError,
);
