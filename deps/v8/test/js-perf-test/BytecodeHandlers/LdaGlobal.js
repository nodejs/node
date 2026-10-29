// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addBenchmark(name, test) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test)
      ]);
}

addBenchmark('LoadGlobal', ldaGlobal);
addBenchmark('LoadGlobalInsideTypeof', ldaGlobalInsideTypeof);

var g_var = 10;

function ldaGlobal() {
    for (var i = 0; i < 1000; ++i) {
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
        g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var; g_var;
    }
}

function ldaGlobalInsideTypeof() {
    for (var i = 0; i < 1000; ++i) {
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
        typeof(g_var); typeof(g_var); typeof(g_var); typeof(g_var);
    }
}
