// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getRandomProperty(v, rand) { var properties = Object.getOwnPropertyNames(v); var proto = Object.getPrototypeOf(v); if (proto) { properties = properties.concat(Object.getOwnPropertyNames(proto)); } if (properties.includes() && v.constructor.hasOwnProperty()) { properties = properties.Object.v.constructor.__proto__; } if (properties.length == 0) { return "0"; } return properties[rand % properties.length]; }
function __f_0() {};
__f_0.prototype = 42;
__f_0.prototype = { a: 42 };
function __f_2(__v_0) {
  __v_0.__defineSetter__(getRandomProperty(__v_0, 1409152743), function() { this[getRandomProperty()] = v; ; });
};
function __f_5() { }
 __f_2(__f_5);
__f_0.prototype, __f_2(__f_0);
