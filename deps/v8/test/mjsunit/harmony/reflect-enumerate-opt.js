// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is adapted from mjsunit/for-in-opt.js.

// Flags: --harmony-proxies --harmony-reflect --allow-natives-syntax


"use strict";

function f(o) {
  var result = [];
  for (var i of Reflect.enumerate(Object(o))) {
    result.push(i);
  }
  return result;
}

assertEquals(["0"], f("a"));
assertEquals(["0"], f("a"));
%OptimizeFunctionOnNextCall(f);
assertEquals(["0","1","2"], f("bla"));

// Test the lazy deopt points.
var keys = ["a", "b", "c", "d"];
var has_keys = [];
var deopt_has = false;
var deopt_enum = false;

var handler = {
  enumerate: function(target) {
    if (deopt_enum) {
      %DeoptimizeFunction(f2);
      deopt_enum = false;
    }
    return keys;
  },

  getPropertyDescriptor: function(k) {
    if (deopt_has) {
      %DeoptimizeFunction(f2);
      deopt_has = false;
    }
    has_keys.push(k);
    return {value: 10, configurable: true, writable: false, enumerable: true};
  }
};

// TODO(neis,cbruni): Enable once the enumerate proxy trap is properly
// implemented.
// var proxy = new Proxy({}, handler);
// var o = {__proto__: proxy};
//
// function f2(o) {
//   var result = [];
//   for (var i of Reflect.enumerate(o)) {
//     result.push(i);
//   }
//   return result;
// }
//
// function check_f2() {
//   assertEquals(keys, f2(o));
//   assertEquals(keys, has_keys);
//   has_keys.length = 0;
// }
//
// check_f2();
// check_f2();
// Test lazy deopt after GetPropertyNamesFast
// %OptimizeFunctionOnNextCall(f2);
// deopt_enum = true;
// check_f2();
// Test lazy deopt after FILTER_KEY
// %OptimizeFunctionOnNextCall(f2);
// deopt_has = true;
// check_f2();
