// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Check that we do appropriate used/unused field accounting
var p = Promise.resolve();
var then = p.then = () => {};

function spread() { return { ...p }; }

%PrepareFunctionForOptimization(spread);
assertEquals({ then }, spread());
assertEquals({ then }, spread());
assertEquals({ then }, spread());
%OptimizeFunctionOnNextCall(spread);
assertEquals({ then }, spread());
