// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('Smi-Value', smiValue);
addBenchmark('Prototype-Chain-Value', functionOnPrototypeValue);

// TODO(all): might be a good idea to also do with receivers: double, HeapObject
// with map, HeapObject, tagged, empty getter / accessor.
// Also, element keyed loads (in another file, probably?) both with strings and
// numbers for normal, cons,and sliced strings (for named properties).
// Also, monomorphic vs poly vs mega.

function smiValue() {
    function constructSmi() {
        this.smi = 0;
    }
    let o = new constructSmi();

    for (var i = 0; i < 1000; ++i) {
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
        o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi; o.smi;
    }
}

function functionOnPrototypeValue() {
    function objectWithPrototypeChain() {
    }
    objectWithPrototypeChain.prototype.__proto__ =
        {__proto__:{__proto__:{__proto__:{f(){}}}}};
    let o = new objectWithPrototypeChain();

    for (var i = 0; i < 1000; ++i) {
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
        o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f; o.f;
    }
}
