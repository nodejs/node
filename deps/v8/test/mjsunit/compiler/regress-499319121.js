// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft-verify-load-store-taggedness --allow-natives-syntax

"use strict";

function* foo() {}
function bar() { return foo.call(5); }

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
bar();

%OptimizeFunctionOnNextCall(bar);
bar();
