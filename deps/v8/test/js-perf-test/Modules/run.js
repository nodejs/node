// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


load('../base.js');

new BenchmarkSuite('BasicExport', [100], [
  new Benchmark('BasicExport', false, false, 0, BasicExport)
]);

new BenchmarkSuite('BasicImport', [100], [
  new Benchmark('BasicImport', false, false, 0, BasicImport)
]);

new BenchmarkSuite('BasicNamespace', [100], [
  new Benchmark('BasicNamespace', false, false, 0, BasicNamespace)
]);

const iterations = 10000;


function BasicExport() {
  let success = false;
  import("basic-export.js").then(m => { m.bench(); success = true; });
  %PerformMicrotaskCheckpoint();
  if (!success) throw new Error(666);
}

function BasicImport() {
  let success = false;
  import("basic-import.js").then(m => { m.bench(); success = true; });
  %PerformMicrotaskCheckpoint();
  if (!success) throw new Error(666);
}

function BasicNamespace() {
  let success = false;
  import("basic-namespace.js").then(m => { m.bench(); success = true; });
  %PerformMicrotaskCheckpoint();
  if (!success) throw new Error(666);
}


var success = true;

function PrintResult(name, result) {
  print(name + '-Modules(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
