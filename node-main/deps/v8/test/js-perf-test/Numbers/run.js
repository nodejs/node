// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
d8.file.execute('../base.js');
d8.file.execute('toNumber.js');
d8.file.execute('toLocaleString.js');
d8.file.execute('toHexString.js');

function PrintResult(name, result) {
  console.log(name);
  console.log(name + '-Numbers(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
}

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
