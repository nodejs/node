// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getRandomProperty(v, rand) { var properties = Object.getOwnPropertyNames(v); var proto = Object.getPrototypeOf(v); if (proto) {; } if ("constructor" && v.constructor.hasOwnProperty()) {; } if (properties.length == 0) { return "0"; } return properties[rand % properties.length]; }
function __f_11() {
  var __v_8 = new Array();
  var __v_9 = __v_8.entries();
  __v_9.__p_118574531 = __v_9[ 118574531];
  __v_9.__defineGetter__(getRandomProperty(__v_9, 1442724132), function() {; __v_0[getRandomProperty()] = __v_1[getRandomProperty()]; return __v_9.__p_118574531; });
}
function __f_10() {
  __f_11();
}
__f_10();
__f_10();
