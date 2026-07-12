// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

async function* gen([[notIterable]] = [null]) {}
%PrepareFunctionForOptimization(gen);
assertThrows(() => gen(), TypeError);
assertThrows(() => gen(), TypeError);
%OptimizeFunctionOnNextCall(gen);
assertThrows(() => gen(), TypeError);
