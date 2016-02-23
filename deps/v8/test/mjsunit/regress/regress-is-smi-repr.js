// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

var global;

function g() { global = this; }
Object.defineProperty(Number.prototype, "prop", { get: g });
function f(s) { s.prop; }

f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
f(1);
