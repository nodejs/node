// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function getRandomProperty(v, rand) { var properties = Object.getOwnPropertyNames(v); var proto = Object.getPrototypeOf(v); if (proto) {; } if ("constructor" && v.constructor.hasOwnProperty()) {; } if (properties.length == 0) { return "0"; } return properties[rand % properties.length]; }
var __v_4 = {};

__v_2 = {
    FAST_ELEMENTS() {
          return {
            get 0() {
            }          };
    }  ,
  Arguments: {
    FAST_SLOPPY_ARGUMENTS_ELEMENTS() {
      var __v_11 = (function( b) { return arguments; })("foo", NaN, "bar");
      __v_11.__p_2006760047 = __v_11[getRandomProperty( 2006760047)];
      __v_11.__defineGetter__(getRandomProperty( 1698457573), function() {  gc(); __v_4[ 1486458228] = __v_2[ 1286067691]; return __v_11.__p_2006760047; });
;
Array.prototype.includes.call(__v_11);
    },
    Detached_Float64Array() {
    }  }
};
function __f_3(suites) {
  Object.keys(suites).forEach(suite => __f_4(suites[suite]));
  function __f_4(suite) {
    Object.keys(suite).forEach(test => suite[test]());
  }
}
__f_3(__v_2);
