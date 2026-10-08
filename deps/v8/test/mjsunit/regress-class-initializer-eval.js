// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

let initializer_ref;
function run() {
  let c = class {
    x = eval("");
  };
  new c;
  let initializer = %GetInitializerFunction(c);
  initializer_ref = new WeakRef(initializer);
}

async function test() {
  run();

  // Run async GC to collect the class and its initializer.
  // This will run without stack pointers on a clean stack.
  await gc({type: 'major', execution: 'async'});

  if (initializer_ref.deref() !== undefined) {
    throw new Error("Initializer function should have been GCed");
  }
}

test();
