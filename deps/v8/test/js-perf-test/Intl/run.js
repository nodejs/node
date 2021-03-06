// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
load('../base.js');
load('constructor.js');

function PrintResult(name, result) {
  console.log(name + '-Intl(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
}

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
