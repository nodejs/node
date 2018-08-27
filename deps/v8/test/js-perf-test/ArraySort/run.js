// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('../base.js');
load(arguments[0] + '.js')

function benchy(name, test, testSetup, testTearDown) {
  new BenchmarkSuite(name, [1000],
      [
        new Benchmark(name, false, false, 0, test, testSetup, testTearDown)
      ]);
}

function PrintResult(name, result) {
  print(name + '-ArraySort(Score): ' + result);
}

function PrintStep(name) {}

function PrintError(name, error) {
  PrintResult(name, error);
}

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError,
                           NotifyStep: PrintStep });
