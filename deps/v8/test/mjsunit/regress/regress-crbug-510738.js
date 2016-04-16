// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function check(f, result) {
  %OptimizeFunctionOnNextCall(f);
  assertEquals(result, f());
}

var x = 17;
function generic_load() { return x; }
check(generic_load, 17);

function generic_store() { x = 13; return x; }
check(generic_store, 13);
