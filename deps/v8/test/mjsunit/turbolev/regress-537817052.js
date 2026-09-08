// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function Box(width) {
  this.width = width;
}

function load(box) {
  box = box.width;
  box = box.width;
}

%PrepareFunctionForOptimization(load);
load(new Box(5));
%OptimizeFunctionOnNextCall(load);
assertThrows(() => load(), TypeError);
