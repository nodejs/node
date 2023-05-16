// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  do {
  } while (true);
}

function boom(x) {
  switch (x) {
    case 1:
    case f():
      return;
  }
};
%PrepareFunctionForOptimization(boom);
%OptimizeFunctionOnNextCall(boom);
boom(1);
