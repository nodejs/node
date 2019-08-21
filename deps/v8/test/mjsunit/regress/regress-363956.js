// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Fuu() {
  this.x = this.x.x;
};
%PrepareFunctionForOptimization(Fuu);
Fuu.prototype.x = {
  x: 1
};
new Fuu();
new Fuu();
%OptimizeFunctionOnNextCall(Fuu);
new Fuu();
