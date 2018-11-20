// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The bug was that destructuring assignments which occur inside a lazy arrow
// function parameter list were not rewritten.

// Repro from the bug (slightly modified so that it doesn't produce a run-time
// exception).
(({x = {} = {}}) => {})({});

// ... and without the parens.
let a0 = ({x = {} = {}}) => {};
a0({});

// Testing that the destructuring assignments also work properly. The semantics
// are: The value of the destructuring assignment is an object {myprop: 2115}
// and 2115 also gets assigned to global_side_assignment. So the default value
// for x is {myprop: 2115}. This is the value which x will have if the function
// is called with an object which doesn't have property x.
let called = false;
let global_side_assignment = undefined;
(({x = {myprop: global_side_assignment} = {myprop: 2115}}) => {
  assertTrue('myprop' in x);
  assertEquals(2115, x.myprop);
  called = true;
})({});
assertTrue(called);
assertEquals(2115, global_side_assignment);

// If the parameter is an object which has property x, the default value is not
// used.
called = false;
global_side_assignment = undefined;
(({x = {myprop: global_side_assignment} = {myprop: 2115}}) => {
  assertEquals(3000, x);
  called = true;
})({x: 3000});
assertTrue(called);
// Global side assignment doesn't happen, since the default value was not used.
assertEquals(undefined, global_side_assignment);

// Different kinds of lazy arrow functions (it's actually a bit weird that the
// above functions are lazy, since they are parenthesized).
called = false;
global_side_assignment = undefined;
let a1 = ({x = {myprop: global_side_assignment} = {myprop: 2115}}) => {
  assertTrue('myprop' in x);
  assertEquals(2115, x.myprop);
  called = true;
}
a1({});
assertTrue(called);
assertEquals(2115, global_side_assignment);

called = false;
global_side_assignment = undefined;
let a2 = ({x = {myprop: global_side_assignment} = {myprop: 2115}}) => {
  assertEquals(3000, x);
  called = true;
}
a2({x: 3000});
assertTrue(called);
assertEquals(undefined, global_side_assignment);

// We never had a problem with non-arrow functions, but testing them too for
// completeness.
called = false;
global_side_assignment = undefined;
function f1({x = {myprop: global_side_assignment} = {myprop: 2115}}) {
  assertTrue('myprop' in x);
  assertEquals(2115, x.myprop);
  assertEquals(2115, global_side_assignment);
  called = true;
}
f1({});
assertTrue(called);
assertEquals(2115, global_side_assignment);

called = false;
global_side_assignment = undefined;
function f2({x = {myprop: global_side_assignment} = {myprop: 2115}}) {
  assertEquals(3000, x);
  called = true;
}
f2({x: 3000});
assertTrue(called);
assertEquals(undefined, global_side_assignment);
