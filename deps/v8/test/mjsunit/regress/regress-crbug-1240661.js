// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --gdbjit --allow-natives-syntax

let f = new Function("boom");

%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
assertThrows(f, ReferenceError);
