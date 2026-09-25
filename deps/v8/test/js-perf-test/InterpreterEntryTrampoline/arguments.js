// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('Calls-No-Argument', callsNoArgument);
addBenchmark('Calls-One-Argument', callsOneArgument);
addBenchmark('Calls-Six-Arguments', callsSixArguments);
addBenchmark('Calls-With-Receiver', callsWithReceiver);

function callsNoArgument() {
    function f() {
        return 0;
    }

    for (var i = 0; i < 1000; ++i) {
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
        f(); f(); f(); f(); f(); f(); f(); f(); f();
    }
}

function callsOneArgument() {
    function f(a) {
        return a;
    }
    let z = 0;

    for (var i = 0; i < 1000; ++i) {
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
        f(z); f(z); f(z); f(z); f(z); f(z); f(z); f(z);
    }
}

function callsSixArguments() {
    function g(a,b,c,d,e,f) {
        return c;
    }
    let a = 0;
    let b = 1;
    let c = 2;
    let d = 3;
    let e = 4;
    let f = 5;

    for (var i = 0; i < 1000; ++i) {
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
        g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f); g(a,b,c,d,e,f);
    }
}

function callsWithReceiver() {
    function constructObject() {
        this.f = function() {
            return 0;
        };
    }
    let o = new constructObject();

    for (var i = 0; i < 1000; ++i) {
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
        o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f(); o.f();
    }
}
