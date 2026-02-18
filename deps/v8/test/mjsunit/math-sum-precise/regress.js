// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise --allow-natives-syntax

let r = ([2.1]).slice(3);
Math.sumPrecise(r);

const v0 = [4294967296];
v0[0] = v0;
%SetAllocationTimeout(-1, 1);
assertThrows(() => Math.sumPrecise(v0), TypeError);
