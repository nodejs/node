// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-lazy

load('test/mjsunit/test-async.js');

// For regression testing, it's important that these functions are:
// 1) toplevel
// 2) arrow functions with single-expression bodies
// 3) eagerly compiled

let f = ({a = (({b = {a = c} = {
  a: 0x1234
}}) => 1)({})}, c) => 1;

assertThrows(() => f({}), ReferenceError);

let g = ({a = (async ({b = {a = c} = {
  a: 0x1234
}}) => 1)({})}, c) => a;

testAsync(assert => {
  assert.plan(1);
  g({}).catch(e => {
    assert.equals("ReferenceError", e.name);
  });
});
