// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var list = [];
function log(item) { list.push(item); }
async function f() {
  try {
    let namespace = await import(/a/);
  } catch(e) {
    log(1);
  }
}
f();

async function importUndefined() {
  try {
    await import({ get toString() { return undefined; }})
  } catch(e) {
    log(2);
  }
}

function g() {
  let namespace = Promise.resolve().then(importUndefined);
}
g();
%PerformMicrotaskCheckpoint();
assertEquals(list, [1,2]);
