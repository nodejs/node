// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let transition = false;
let tagged_value;
let double_value;

function tagged_reflect() {
  const a = eval('tagged_inner.arguments');
  if (transition) {
    a[1][0] = tagged_value;
    a[1][1] = tagged_value;
  } else {
    a[1][0] = 3.3;
    a[1][1] = 3.3;
  }
}

function tagged_next(iterator) {
  return iterator.next().value;
}

function tagged_inner(iterator, array) {
  tagged_reflect();
  return tagged_next(iterator);
}

function tagged_outer() {
  const array = [1.1, 2.2];
  const iterator = array.values();
  return tagged_inner(iterator, array);
}

function double_reflect() {
  const a = eval('double_inner.arguments');
  if (transition) {
    a[1][0] = double_value;
    a[1][1] = double_value;
  } else {
    a[1][0] = 3;
    a[1][1] = 3;
  }
}

function double_next(iterator) {
  return iterator.next().value;
}

function double_inner(iterator, array) {
  double_reflect();
  return double_next(iterator);
}

function double_outer() {
  const array = [1, 2];
  const iterator = array.values();
  return double_inner(iterator, array);
}

const marker = {};
tagged_value = marker;
double_value = 1.5;

for (const f of [tagged_reflect, tagged_next, tagged_inner, tagged_outer,
                 double_reflect, double_next, double_inner, double_outer]) {
  %PrepareFunctionForOptimization(f);
}

tagged_outer();
tagged_outer();
double_outer();
double_outer();

%OptimizeMaglevOnNextCall(tagged_outer);
%OptimizeMaglevOnNextCall(double_outer);

transition = true;
assertSame(marker, tagged_outer());
assertEquals(1.5, double_outer());
