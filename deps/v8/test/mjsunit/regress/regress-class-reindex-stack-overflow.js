// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let non_callable = 1;
let a = {};
a.b = a;
let chain = "a" + ".b".repeat(1500);

let C = eval(`(class {
  [class extends (${chain}, Object) { #f = () => {}; }] = non_callable();
})`);

function recurse() {
  try {
    recurse();
  } catch (e) {
    new C();
  }
}
try {
  recurse();
} catch (e) {}
