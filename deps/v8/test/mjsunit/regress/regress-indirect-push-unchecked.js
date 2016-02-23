// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = [1.5];

function p() {
  Array.prototype.push.call(a, 1.7);
}

p();
p();
p();
%OptimizeFunctionOnNextCall(p);
p();
a.push({});
p();
assertEquals(1.7, a[a.length - 1]);
