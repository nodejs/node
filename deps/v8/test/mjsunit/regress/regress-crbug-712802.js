// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(...args) { return Array.isArray(args); }

assertTrue(foo());
assertTrue(foo());
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo());
