// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

d8.file.execute('classes.js');

function CreateBenchmark(name, optimize) {
  let klass;
  let array;

  switch (name) {
    case "InitializeSinglePublicFieldClass":
      klass = EvaluateSinglePublicFieldClass();
      break;
    case "InitializeMultiPublicFieldClass":
      klass = EvaluateMultiPublicFieldClass();
      break;
    case "InitializeSinglePrivateFieldClass":
      klass = EvaluateSinglePrivateFieldClass();
      break;
    case "InitializeMultiPrivateFieldClass":
      klass = EvaluateMultiPrivateFieldClass();
      break;
    case "InitializeSinglePrivateMethodClass":
      klass = EvaluateSinglePrivateMethodClass();
      break;
    case "InitializeMultiPrivateMethodClass":
      klass = EvaluateMultiPrivateMethodClass();
      break;
    case "InitializeSingleComputedFieldClass":
      klass = EvaluateSingleComputedFieldClass();
      break;
    case "InitializeMultiComputedFieldClass":
      klass = EvaluateMultiComputedFieldClass();
      break;
    default:
      throw new Error("Unknown optimization configuration " + arguments.join(' '));
  }

  if (optimize) {
    %PrepareFunctionForOptimization(klass);
  } else {
    %NeverOptimizeFunction(klass);
  }

  function setUp() {
    array = [new klass(), new klass()];
    // Populate the array first to reduce the impact of
    // array allocations.
    for (let i = 0; i < LOCAL_ITERATIONS - 2; ++i) {
      array.push(array[0]);
    }
    if (optimize) {
      %OptimizeFunctionOnNextCall(klass);
    }
  }

  function runBenchmark() {
    for (let i = 0; i < LOCAL_ITERATIONS; ++i) {
      array[i] = new klass();
    }
  }

  function tearDown() {
    if (array.length < 3) {
      throw new Error(`Check failed, array length ${array.length}`);
    }

    for (const instance of array) {
      if (!instance.check())
        throw new Error(`instance.check() failed`);
    }
  }

  const DETERMINISTIC_RUNS = 1;
  const LOCAL_ITERATIONS = 10000;

  const benchName = `${name}${optimize ? "Opt" : "NoOpt"}`
  new BenchmarkSuite(benchName, [1000], [
    new Benchmark(
      benchName,
      false, false, DETERMINISTIC_RUNS, runBenchmark, setUp, tearDown)
  ]);
}

let optimize;
switch (arguments[1]) {
  case 'opt':
    optimize = true;
    break;
  case 'noopt':
    optimize = false;
    break;
  default:
    throw new Error("Unknown optimization configuration " + arguments.join(' '));
}

CreateBenchmark("InitializeSinglePublicFieldClass", optimize);
CreateBenchmark("InitializeMultiPublicFieldClass", optimize);
CreateBenchmark("InitializeSinglePrivateFieldClass", optimize);
CreateBenchmark("InitializeMultiPrivateFieldClass", optimize);
CreateBenchmark("InitializeSinglePrivateMethodClass", optimize);
CreateBenchmark("InitializeMultiPrivateMethodClass", optimize);
CreateBenchmark("InitializeSingleComputedFieldClass", optimize);
CreateBenchmark("InitializeMultiComputedFieldClass", optimize);
