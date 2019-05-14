// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


load('../base.js');

load('ctor.js');
load('exec.js');
load('flags.js');
load('inline_test.js')
load('match.js');
load('replace.js');
load('search.js');
load('split.js');
load('test.js');
load('slow_exec.js');
load('slow_flags.js');
load('slow_match.js');
load('slow_replace.js');
load('slow_search.js');
load('slow_split.js');
load('slow_test.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-RegExp(Score): ' + result);
}


function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
