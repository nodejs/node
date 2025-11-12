// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --assert-types

class A {};

class B extends A {};

B.__proto__ = null;

%PrepareFunctionForOptimization(B);
try { new B(); } catch {}
%OptimizeFunctionOnNextCall(B);
try { new B(); } catch {}
