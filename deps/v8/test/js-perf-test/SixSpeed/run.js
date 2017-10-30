// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('../base.js');
load(arguments[0] + '.js');

var success = true;

function PrintResult(name, result) {
  print(name + '(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

BenchmarkSuite.config.doWarmup = true;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
