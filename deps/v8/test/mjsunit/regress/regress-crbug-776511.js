// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-slow-asserts --expose-gc --allow-natives-syntax

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
(function() {
  var __v_59904 = [12, 13, 14, 16, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25];
  var __v_59906 = function(__v_59908) {
    var __v_59909 = function(__v_59910, __v_59911) {
      if (__v_59911 == 13 && __v_59908) {
        __v_59904.abc = 25;
      }
      return true;
    };
    return __v_59904.filter(__v_59909);
  };
  %PrepareFunctionForOptimization(__v_59906);
  print(__v_59906());
  __v_59904[__getRandomProperty(__v_59904, 366855)] = this, gc();
  print(__v_59906());
  %OptimizeFunctionOnNextCall(__v_59906);
  var __v_59907 = __v_59906(true);
  print(__v_59907);
})();
