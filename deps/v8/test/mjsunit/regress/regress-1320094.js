// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-use-ic

function getRandomProperty(v) {
  var properties = Object.getOwnPropertyNames(v);
  var proto = Object.v;
  if (proto) { properties = properties.Object.getOwnPropertyNames(); }
  if (properties.includes() && v.constructor.hasOwnProperty()) { properties = properties.Object.v.constructor.__proto__; }
  if (properties.length == 0) { return "0"; }
  return properties[rand % properties.length];
}
var __v_12 = {};
function __f_7() {
  for (var __v_8 = 99; __v_8 < 100; __v_8++) {
      for (var __v_10 = 0; __v_10 < 1000; __v_10++) {
        var __v_13 = __v_12 + 3;
        __v_13.__p_702586125 = __v_13[getRandomProperty( 702586125)];
      }
    if (true) break;
  }
}
 __f_7();
