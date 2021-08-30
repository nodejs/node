// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

d8.file.execute('../base.js');
d8.file.execute('common.js');
d8.file.execute('map.js');
d8.file.execute('set.js');
d8.file.execute('weakmap.js');
d8.file.execute('weakset.js');


var success = true;

function PrintResult(name, result) {
  print(name + '-Collections(Score): ' + result);
}


function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError });
