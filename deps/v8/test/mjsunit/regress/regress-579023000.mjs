// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

import * as ns1 from "./regress-579023000.mjs";
export const x = 42;

function read1(o, do_read) {
  if (do_read) return o.x;
  return 0;
}

%PrepareFunctionForOptimization(read1);
for (let i = 0; i < 5; i++) {
  assertEquals(42, read1(ns1, true));
}

// Construct a dictionary-mode object with > kMaxNumberOfDescriptors (1020)
// enumerable properties and an indexed element so SetDataProperties takes the
// runtime path.
const big_dict = {};
for (let i = 0; i < 1025; i++) {
  big_dict["p" + i] = 1;
}
delete big_dict.p0;
big_dict.p0 = 1;
big_dict[0] = 1;

assertThrows(() => Object.assign(ns1, big_dict), TypeError);

%OptimizeFunctionOnNextCall(read1);
assertEquals(0, read1(ns1, false));
assertEquals(42, read1(ns1, true));

// Also test normalizing a fast JSModuleNamespace via private field addition
// overflowing kMaxNumberOfDescriptors (1020).
const ns2 = await import("data:text/javascript,export const x = 42;");

function read2(o, do_read) {
  if (do_read) return o.x;
  return 0;
}

// Transition ns2 to a fast prototype map.
for (let i = 0; i < 5; i++) {
  assertEquals(42, read2(ns2, true));
}

// Add 1018 private fields (2 existing descriptors + 1018 = 1020 =
// kMaxNumberOfDescriptors).
let fields = "";
for (let i = 0; i < 1018; i++) {
  fields += "#f" + i + " = 1;\n";
}
class Base {
  constructor(o) {
    return o;
  }
}
const Add1018PrivateFields = eval(`(class extends Base { ${fields} })`);
new Add1018PrivateFields(ns2);

// Warm up LoadIC on the 1020-descriptor fast prototype map.
%PrepareFunctionForOptimization(read2);
for (let i = 0; i < 5; i++) {
  assertEquals(42, read2(ns2, true));
}

// Adding one more private field overflows kMaxNumberOfDescriptors and
// normalizes ns2 to dictionary mode.
class AddOneMorePrivateField extends Base {
  #extra = 1;
}
new AddOneMorePrivateField(ns2);

%OptimizeFunctionOnNextCall(read2);
assertEquals(0, read2(ns2, false));
assertEquals(42, read2(ns2, true));
