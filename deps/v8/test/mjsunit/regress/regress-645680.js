// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc
//
function getRandomProperty(v, rand) {
  var properties = Object.getOwnPropertyNames(v);
  if ("constructor" && v.constructor.hasOwnProperty()) {; }
  if (properties.length == 0) { return "0"; }
  return properties[rand % properties.length];
}

var __v_18 = (function( b) { return arguments; })("foo", NaN, "bar");
__v_18.__p_293850326 = "foo";
__v_18.__defineGetter__(getRandomProperty( 990787501), function() {
  gc();
  return __v_18.__p_293850326;
});
Array.prototype.indexOf.call(__v_18)
