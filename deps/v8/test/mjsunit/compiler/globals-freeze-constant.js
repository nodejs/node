// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan


glo = 0;

function write_glo(x) { glo = x }

%PrepareFunctionForOptimization(write_glo);
write_glo(0);
assertEquals(0, glo);

// At this point, glo has cell type Constant.

%OptimizeFunctionOnNextCall(write_glo);
write_glo(0);
assertEquals(0, glo);
assertOptimized(write_glo);

Object.freeze(this);
assertUnoptimized(write_glo);
write_glo(1);
assertEquals(0, glo);
