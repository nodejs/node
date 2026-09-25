// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

load("test/mjsunit/regress-401652934-2.js")

let testVar;
testVar = 0 * Math.PI;

checkTestVar();
checkTestVar();
