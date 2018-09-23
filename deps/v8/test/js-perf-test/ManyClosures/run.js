// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


load('../base.js');
load('create-many-closures.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ManyClosures(Score): ' + result);
}


function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
