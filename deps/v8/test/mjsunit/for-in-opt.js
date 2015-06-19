// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-proxies --allow-natives-syntax

"use strict";

// Test non-JSObject receiver.
function f(o) {
  var result = [];
  for (var i in o) {
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


var proxy = Proxy.create(handler);
var o = {__proto__: proxy};

function f2(o) {
  var result = [];
  for (var i in o) {
    result.push(i);
  }
  return result;
}

function check_f2() {
  assertEquals(keys, f2(o));
  assertEquals(keys, has_keys);
  has_keys.length = 0;
}

check_f2();
check_f2();
// Test lazy deopt after GetPropertyNamesFast
%OptimizeFunctionOnNextCall(f2);
deopt_enum = true;
check_f2();
// Test lazy deopt after FILTER_KEY
%OptimizeFunctionOnNextCall(f2);
deopt_has = true;
check_f2();

function f3(o) {
  for (var i in o) {
  }
}

f3({__proto__:{x:1}});
f3({__proto__:{x:1}});
%OptimizeFunctionOnNextCall(f3);
f3(undefined);
f3(null);
