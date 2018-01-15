// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load('../base.js');

let listener = null;

load('debugger.js');
load('runtime.js');

let success = true;
let lastId = 0;

function PrintResult(name, result) {
  print(name + '-Inspector(Score): ' + result);
}

function PrintStep(name) {}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}


BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;

BenchmarkSuite.RunSuites({ NotifyResult: PrintResult,
                           NotifyError: PrintError,
                           NotifyStep: PrintStep });

function SendMessage(method, params) {
  let obj = {id: ++lastId, method: method};
  if (params) {
    obj.params = params;
  }
  send(JSON.stringify(obj));
}

function receive(message) {
  if (listener) {
    listener(JSON.parse(message));
  }
}
