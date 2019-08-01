// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* get() {
  for (let x of [1,2,3]) {
    yield;
    get = [];
  }
}
%OptimizeFunctionOnNextCall(get);
get();
