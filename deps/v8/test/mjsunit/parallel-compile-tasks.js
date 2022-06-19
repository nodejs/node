// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --compiler-dispatcher --parallel-compile-tasks-for-eager-toplevel --use-external-strings

(function(a) {
 assertEquals(a, "IIFE");
})("IIFE");

(function(a, ...rest) {
 assertEquals(a, 1);
 assertEquals(rest.length, 2);
 assertEquals(rest[0], 2);
 assertEquals(rest[1], 3);
})(1,2,3);

var outer_var = 42;

function lazy_outer() {
 return 42;
}

var eager_outer = (function() { return 42; });

(function() {
 assertEquals(outer_var, 42);
 assertEquals(lazy_outer(), 42);
 assertEquals(eager_outer(), 42);
})();

var gen = (function*() {
 yield 1;
 yield 2;
})();

assertEquals(gen.next().value, 1);
assertEquals(gen.next().value, 2);

var result = (function recursive(a=0) {
 if (a == 1) {
  return 42;
 }
 return recursive(1);
})();

assertEquals(result, 42);

var a = 42;
var b;
var c = (a, b = (function z(){ return a+1; })());
assertEquals(b, 43);
assertEquals(c, 43);
var c = (a, b = (function z(){ return a+1; })()) => { return b; };
assertEquals(c(314), 315);

// http://crbug.com/898076
(function() {
  class foo {};
});  // Don't call IIFE so that it is compiled during idle time

// http://crbug.com/900535
(function() {
  "use asm";
  function bar(i, j) {
    i = i|0;
    j = j|0;
  }
  return {bar: bar};
});  // Don't call IIFE so that it is compiled during idle time
