// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// TurboFan optimizes integer loops. These tests check that we compute
// the correct upper and lower bounds.
function positive_increment() {
  for (var i = 5; i < 10; i++) {
    if (i < 0) return false;
    if (i > 20) return false;
    if (i === 7) return true;
  }
  return false;
}

function positive_increment_strict() {
  for (var i = 5; i < 10; i++) {
    if (i < 0) return false;
    if (i === 10) return false;
  }
  return true;
}

function positive_increment_non_strict() {
  for (var i = 5; i <= 10; i++) {
    if (i < 0) return false;
    if (i === 10) return true;
  }
  return false;
}

function negative_increment() {
  for (var i = 10; i > 5;) {
    if (i < 0) return false;
    if (i > 20) return false;
    if (i === 7) return true;
    i -= 1;
  }
  return false;
}

function positive_decrement() {
  for (var i = 10; i > 5; i--) {
    if (i < 0) return false;
    if (i === 7) return true;
  }
  return false;
}

function positive_decrement_strict() {
  for (var i = 10; i > 5; i--) {
    if (i < 0) return false;
    if (i === 5) return false;
  }
  return true;
}
function positive_decrement_non_strict() {
  for (var i = 10; i >= 5; i--) {
    if (i < 0) return false;
    if (i === 5) return true;
  }
  return false;
}

function negative_decrement() {
  for (var i = 5; i < 10;) {
    if (i < 0) return false;
    if (i === 7) return true;
    i -= -1;
  }
  return false;
}

function variable_bound() {
  for (var i = 5; i < 10; i++) {
    for (var j = 5; j < i; j++) {
      if (j < 0) return false;
      if (j === 7) return true;
    }
  }
  return false;

}

function test(f) {
  f();
  assertTrue(f());
  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
}

test(positive_increment);
test(positive_increment_strict);
test(positive_increment_non_strict);
test(negative_increment);
test(positive_decrement);
test(positive_decrement_strict);
test(positive_decrement_non_strict);
test(negative_decrement);
test(variable_bound);
