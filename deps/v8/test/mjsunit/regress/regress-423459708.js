// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --stress-compaction

function makeObj(fun) {
  let o1 = { str: "A" };

  let o2 = { str: "BBBBBBBBBBBBBB" };
  o2.f = {};

  let arr = [o1,o2];
  return fun(arr);
}
const json = makeObj((a) => JSON.stringify(a));

%SetAllocationTimeout(1, 2);

const obj = JSON.parse(json);
assertEquals(makeObj((a) => a), obj);
