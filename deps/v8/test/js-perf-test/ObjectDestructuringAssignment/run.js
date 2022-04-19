// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const props = {
  key: 'abc',
  ref: 1234,
  a: 10,
  b: 20,
  c: 30,
  d: 40,
  e: 50
};

// ----------------------------------------------------------------------------
// Benchmark: Babel
// ----------------------------------------------------------------------------

function _objectWithoutProperties(source, excluded) {
  var target = _objectWithoutPropertiesLoose(source, excluded);
  var key, i;
  var sourceSymbolKeys = Object.getOwnPropertySymbols(source);
  for (i = 0; i < sourceSymbolKeys.length; i++) {
    key = sourceSymbolKeys[i];
    if (!Object.prototype.propertyIsEnumerable.call(source, key)) continue;
    target[key] = source[key];
  }
  return target;
}

function _objectWithoutPropertiesLoose(source, excluded) {
  var target = {};
  var sourceKeys = Object.keys(source);
  var key, i;
  for (i = 0; i < sourceKeys.length; i++) {
    key = sourceKeys[i];
    if (excluded.indexOf(key) >= 0) continue;
    target[key] = source[key];
  }
  return target;
}
function Babel() {
  const key = props.key;
  const ref = props.ref;
  const normalizedProps = _objectWithoutProperties(props, ['key', 'ref']);
}

// ----------------------------------------------------------------------------
// Benchmark: ForLoop
// ----------------------------------------------------------------------------

function ForLoop() {
  const key = props.key;
  const ref = props.ref;
  const normalizedProps = {};
  for (let i in props) {
    if (i != 'key' && i != 'ref') {
      normalizedProps[i] = props[i];
    }
  }
}

// ----------------------------------------------------------------------------
// Benchmark: DestructuringAssignment
// ----------------------------------------------------------------------------

function DestructuringAssignment() {
  const {key, ref, ...normalizedProps} = props;
}

// ----------------------------------------------------------------------------
// Setup and Run
// ----------------------------------------------------------------------------

d8.file.execute('../base.js');

var success = true;

function PrintResult(name, result) {
  print(name + '-ObjectDestructuringAssignment(Score): ' + result);
}

function PrintError(name, error) {
  PrintResult(name, error);
  success = false;
}

function CreateBenchmark(name, f) {
  new BenchmarkSuite(name, [100], [new Benchmark(name, false, false, 0, f)]);
}

CreateBenchmark('Babel', Babel);
CreateBenchmark('ForLoop', ForLoop);
CreateBenchmark('DestructuringAssignment', DestructuringAssignment);

BenchmarkSuite.config.doWarmup = undefined;
BenchmarkSuite.config.doDeterministic = undefined;
BenchmarkSuite.RunSuites({NotifyResult: PrintResult, NotifyError: PrintError});
