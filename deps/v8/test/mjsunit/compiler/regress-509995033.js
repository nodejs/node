// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --homomorphic-ic

function f1() {}
function f2() {}
function f3() {}
function f4() {}
function f5() {}

const v1 = new f1(); v1.a = 1;
const v2 = new f2(); v2.a = 1;
const v3 = new f3(); v3.a = 1;
const v4 = new f4(); v4.a = 1;
const v5 = new f5(); v5.a = 1;

function f_opt(o) {
    o.non_existent;
    o.a = 1.1;
    return o.a;
}
%PrepareFunctionForOptimization(f_opt);

assertEquals(1.1, f_opt(v1));
assertEquals(1.1, f_opt(v2));
assertEquals(1.1, f_opt(v3));
assertEquals(1.1, f_opt(v4));
assertEquals(1.1, f_opt(v5));
%OptimizeFunctionOnNextCall(f_opt);
assertEquals(1.1, f_opt(v5));
