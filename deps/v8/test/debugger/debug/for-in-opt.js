// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


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
var property_descriptor_keys = [];
var deopt_enum = false;
var deopt_property_descriptor = false;

var handler = {
  ownKeys() {
    if (deopt_enum) {
      %DeoptimizeFunction(f2);
      deopt_enum = false;
    }
    return keys;
  },
  getOwnPropertyDescriptor(target, k) {
    if (deopt_property_descriptor) {
      %DeoptimizeFunction(f2);
      deopt_property_descriptor = false;
    }
    property_descriptor_keys.push(k);
    return { enumerable: true, configurable: true }
  },
};


var proxy = new Proxy({}, handler);
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
  assertEquals(keys, property_descriptor_keys);
  property_descriptor_keys.length = 0;
}

check_f2();
check_f2();

// Test lazy deopt after ForInEnumerate
%OptimizeFunctionOnNextCall(f2);
deopt_enum = true;
check_f2();

// Test lazy deopt after FILTER_KEY
%OptimizeFunctionOnNextCall(f2);
deopt_property_descriptor = true;
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

// Reliable repro for an issue previously flushed out by GC stress.
var p = {x: "x"}

function f4(o, p) {
  var result = [];
  for (var i in o) {
    var j = p.x + "str";
    result.push(i);
  }
  return result;
}

function check_f4() {
  assertEquals(keys, f4(o, p));
  assertEquals(keys, property_descriptor_keys);
  property_descriptor_keys.length = 0;
}

check_f4();
check_f4();

%OptimizeFunctionOnNextCall(f4);

p.y = "y";  // Change map, cause eager deopt.
check_f4();

// Repro for Turbofan equivalent.
var x;
var count = 0;

var Debug = debug.Debug;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    %DeoptimizeFunction(f5);
  }
}

var handler3 = {
  ownKeys() { return ["a", "b"] },
  getOwnPropertyDescriptor(target, k) {
    if (k == "a") count++;
    if (x) %ScheduleBreak()
    return { enumerable: true, configurable: true }
  }
};

var proxy3 = new Proxy({}, handler3);
var o3 = {__proto__: proxy3};

function f5() {
  for (var p in o3) {
    print(p);
  }
}

x = false;

f5(); f5(); f5();
%OptimizeFunctionOnNextCall(f5);
x = true;
count = 0;
Debug.setListener(listener);
f5();
Debug.setListener(null);
assertEquals(1, count);
