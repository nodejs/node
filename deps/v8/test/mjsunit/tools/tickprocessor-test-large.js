// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sample file for creating tickprocessor-test.log
"use strict"

let result;

function loop() {
  let result = 0;
  for (let i = 0; i < 10_000_000; i++) {
    result = add(result, 1);
  }
  return result;
}

function add(a, b) {
  return a + b;
}


result = loop();
result = loop();

// Cause some IC misses
function read_monomorphic(o) {
  return o.value
}

function read_polymorphic(o) {
  return o.value
}

function read_megamorphic(o) {
  return o.value
}

const objects = [];
for (let i = 0; i < 100; i++) {
  const object = {};
  object['key' + i ];
  object['value'] = 1 + i/100;
  objects.push(object)
}

function ics() {
  result = 0;
  for (let i = 0; i < objects.length; i++) {
    result += read_monomorphic(objects[0]);
    result += read_polymorphic(objects[i % 3])
    result += read_megamorphic(objects[i]);
  }
}

for (let i = 0; i < 10_000; i++) {
  ics();
}
