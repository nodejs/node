// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const input = {
  a: 1,
  b: 2,
  c: 3,
  ['d']: 4,
  1: 5
};


// ----------------------------------------------------------------------------
// Benchmark: Babel
// ----------------------------------------------------------------------------

function _extends(target) {
  for (var i = 1; i < arguments.length; i++) {
    var source = arguments[i];
    for (var key in source) {
      if (Object.prototype.hasOwnProperty.call(source, key)) {
        target[key] = source[key];
      }
    }
  }
  return target;
};

function Babel() {
  const result = _extends({}, input);
  if (Object.keys(result).length != 5) throw 666;
}

function BabelAndOverwrite() {
  const result = _extends({}, input, {a: 6});
  if (Object.keys(result).length != 5) throw 666;
}

function BabelAndExtend() {
  const result = _extends({}, input, {x: 6, y: 7});
  if (Object.keys(result).length != 7) throw 666;
}

// ----------------------------------------------------------------------------
// Benchmark: Object.assign
// ----------------------------------------------------------------------------


function ObjectAssign() {
  const result = Object.assign({}, input);
  if (Object.keys(result).length != 5) throw 666;
}

function ObjectAssignAndOverwrite() {
  const result = Object.assign({}, input, {a : 6});
  if (Object.keys(result).length != 5) throw 666;
}

function ObjectAssignAndExtend() {
  const result = Object.assign({}, input, {x: 6, y: 7});
  if (Object.keys(result).length != 7) throw 666;
}


// ----------------------------------------------------------------------------
// Benchmark: Object.assign
// ----------------------------------------------------------------------------


function ObjectSpread() {
  const result = { ...input };
  if (Object.keys(result).length != 5) throw 666;
}

function ObjectSpreadAndOverwrite() {
  const result = { ...input, a: 6 };
  if (Object.keys(result).length != 5) throw 666;
}

function ObjectSpreadAndExtend() {
  const result = { ...input, x: 6, y: 7 };
  if (Object.keys(result).length != 7) throw 666;
}

// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

d8.file.execute('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ObjectLiteralSpread(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [100], [ new Benchmark(name, false, false, 0, f) ]);
}

CreateBenchmark('Babel', Babel);
CreateBenchmark('BabelAndOverwrite', BabelAndOverwrite);
CreateBenchmark('BabelAndExtend', BabelAndExtend);
CreateBenchmark('ObjectAssign', ObjectAssign);
CreateBenchmark('ObjectAssignAndOverwrite', ObjectAssignAndOverwrite);
CreateBenchmark('ObjectAssignAndExtend', ObjectAssignAndExtend);
CreateBenchmark('ObjectSpread', ObjectSpread);
CreateBenchmark('ObjectSpreadAndOverwrite', ObjectSpreadAndOverwrite);
CreateBenchmark('ObjectSpreadAndExtend', ObjectSpreadAndExtend);

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
