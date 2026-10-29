// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-escape

function f() {
  with ({ value:"foo" }) { return value; }
}
%PrepareFunctionForOptimization(f);
assertEquals("foo", f());
%OptimizeFunctionOnNextCall(f);
assertEquals("foo", f());
