// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --omit-default-ctors --allow-natives-syntax --assert-types

class C0 {}
const proxy = new Proxy(C0, {});
class C1 extends proxy {}

%PrepareFunctionForOptimization(C1);
new C1();
%OptimizeFunctionOnNextCall(C1);
new C1();
