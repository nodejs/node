// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

var a = [];
Object.defineProperty(a, "0", {configurable: false, value: 10});
assertEquals(1, a.length);
var setter = ()=>{ a.length = 0; };
%PrepareFunctionForOptimization(setter);
assertThrows(setter);
assertThrows(setter);
%OptimizeFunctionOnNextCall(setter);
assertThrows(setter);
