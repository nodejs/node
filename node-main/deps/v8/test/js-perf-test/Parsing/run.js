// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


d8.file.execute("../base.js");

const iterations = 100;
let code;

d8.file.execute("comments.js");
d8.file.execute("strings.js");
d8.file.execute("arrowfunctions.js")

var success = true;

function PrintResult(name, result) {
  print(name + "-Parsing(Score): " + result);
}


function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
