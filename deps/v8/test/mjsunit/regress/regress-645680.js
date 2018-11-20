// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --allow-natives-syntax

function getRandomProperty(v, rand) {
  var properties = Object.getOwnPropertyNames(v);
  if ("constructor" && v.constructor.hasOwnProperty()) {; }
  if (properties.length == 0) { return "0"; }
  return properties[rand % properties.length];
}

var args = (function( b) { return arguments; })("foo", NaN, "bar");
args.__p_293850326 = "foo";
%HeapObjectVerify(args);
args.__defineGetter__(getRandomProperty( 990787501), function() {
  gc();
  return args.__p_293850326;
});
%HeapObjectVerify(args);
Array.prototype.indexOf.call(args)
