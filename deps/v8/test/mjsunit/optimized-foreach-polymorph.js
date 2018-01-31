// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --turbo-inline-array-builtins

var a = [0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0];
var b = [{}, {}];
var c = [,,,,,2,3,4];
var d = [0.5,3,4];
var e = [,,,,0.5,3,4];

// Make sure that calls to forEach handle a certain degree of polymorphism (no
// hole check)
(function() {
  var result = 0;
  var polymorph1 = function(arg) {
    var sum = function(v,i,o) {
      result += i;
    }
    arg.forEach(sum);
  }
  polymorph1(a);
  polymorph1(a);
  polymorph1(b);
  polymorph1(a);
  polymorph1(a);
  %OptimizeFunctionOnNextCall(polymorph1);
  polymorph1(a);
  polymorph1(b);
  assertEquals(1757, result);
})();

// Make sure that calls to forEach handle a certain degree of polymorphism.
(function() {
  var result = 0;
  var polymorph1 = function(arg) {
    var sum = function(v,i,o) {
      result += i;
    }
    arg.forEach(sum);
  }
  polymorph1(a);
  polymorph1(a);
  polymorph1(b);
  polymorph1(a);
  polymorph1(c);
  polymorph1(a);
  %OptimizeFunctionOnNextCall(polymorph1);
  polymorph1(a);
  polymorph1(b);
  assertEquals(1775, result);
})();

// Make sure that calls to forEach with mixed object/double arrays don't inline
// forEach.
(function() {
  var result = 0;
  var polymorph1 = function(arg) {
    var sum = function(v,i,o) {
      result += i;
    }
    arg.forEach(sum);
  }
  polymorph1(a);
  polymorph1(a);
  polymorph1(b);
  polymorph1(a);
  polymorph1(d);
  polymorph1(a);
  %OptimizeFunctionOnNextCall(polymorph1);
  polymorph1(a);
  polymorph1(b);
  assertEquals(1760, result);
})();

// Make sure that calls to forEach with double arrays get the right result
(function() {
  var result = 0;
  var polymorph1 = function(arg) {
    var sum = function(v,i,o) {
      result += v;
    }
    arg.forEach(sum);
  }
  polymorph1(d);
  polymorph1(d);
  polymorph1(d);
  %OptimizeFunctionOnNextCall(polymorph1);
  polymorph1(d);
  polymorph1(d);
  assertEquals(37.5, result);
})();

// Make sure that calls to forEach with mixed double arrays get the right result
(function() {
  var result = 0;
  var polymorph1 = function(arg) {
    var sum = function(v,i,o) {
      result += v;
    }
    arg.forEach(sum);
  }
  polymorph1(d);
  polymorph1(e);
  polymorph1(d);
  %OptimizeFunctionOnNextCall(polymorph1);
  polymorph1(d);
  polymorph1(e);
  assertEquals(37.5, result);
})();
