// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var s_uninternalized = "concurrent" + "-skip-finalization";
%InternalizeString(s_uninternalized);

function foo() {}

// Make sure %OptimizeFunctionOnNextCall works with a non-internalized
// string parameter.
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo, s_uninternalized)
