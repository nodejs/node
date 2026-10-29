// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __getProperties(obj) {
  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
    properties.push(name);
  }
  return properties;
}
function __getRandomProperty(obj, seed) {
  let properties = __getProperties(obj);
  return properties[seed % properties.length];
}
let __v_19 = [];
class __c_0 extends Array {}
Object.defineProperty(__v_19, 'constructor', {
  get() {
    return __c_0;
  }
});
Object.defineProperty(__v_19, __getRandomProperty(__v_19, 776790), {
  value: 4294967295
});
assertThrows(() => __v_19.concat([1])[9], RangeError);
