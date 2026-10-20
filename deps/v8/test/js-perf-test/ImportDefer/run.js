// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('../base.js');

new BenchmarkSuite('HotAccessEager', [100], [
  new Benchmark('HotAccessEager', false, false, 0, HotAccessEager)
]);

new BenchmarkSuite('HotAccessDefer', [100], [
  new Benchmark('HotAccessDefer', false, false, 0, HotAccessDefer)
]);

const iterations = 10000;

function HotAccessEager() {
  let success = false;
  import("hotaccess-eager.js").then(m => { m.bench(); success = true; });
  %PerformMicrotaskCheckpoint();
  if (!success) throw new Error("Error evaluating 'hotaccess-eager.js'");
}

function HotAccessDefer() {
  let success = false;
  import("hotaccess-defer.js").then(m => { m.bench(); success = true; });
  %PerformMicrotaskCheckpoint();
  if (!success) throw new Error("Error evaluating 'hotaccess-defer.js'");
}

var success = true;

function PrintResult(name, result) {
  print(name + '-ImportDefer(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
