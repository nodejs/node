// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

d8.file.execute('classes.js');

function CreateBenchmark(name, optimize) {
  let factory;
  let array;

  switch (name) {
    case "EvaluateSinglePublicFieldClass":
      factory = EvaluateSinglePublicFieldClass;
      break;
    case "EvaluateMultiPublicFieldClass":
      factory = EvaluateMultiPublicFieldClass;
      break;
    case "EvaluateSinglePrivateFieldClass":
      factory = EvaluateSinglePrivateFieldClass;
      break;
    case "EvaluateMultiPrivateFieldClass":
      factory = EvaluateMultiPrivateFieldClass;
      break;
    case "EvaluateSinglePrivateMethodClass":
      factory = EvaluateSinglePrivateMethodClass;
      break;
    case "EvaluateMultiPrivateMethodClass":
      factory = EvaluateMultiPrivateMethodClass;
      break;
    case "EvaluateSingleComputedFieldClass":
      factory = EvaluateSingleComputedFieldClass;
      break;
    case "EvaluateMultiComputedFieldClass":
      factory = EvaluateMultiComputedFieldClass;
      break;
    default:
      throw new Error("Unknown optimization configuration " + arguments.join(' '));
  }

  if (optimize) {
    %PrepareFunctionForOptimization(factory);
  } else {
    %NeverOptimizeFunction(factory);
  }

  function setUp() {
    array = [factory(), factory()];
    // Populate the array first to reduce the impact of
    // array allocations.
    for (let i = 0; i < LOCAL_ITERATIONS - 2; ++i) {
      array.push(array[0]);
    }
    if (optimize) {
      %OptimizeFunctionOnNextCall(factory);
    }
  }

  function runBenchmark() {
    for (let i = 0; i < LOCAL_ITERATIONS; ++i) {
      array[i] = factory();
    }
  }

  function tearDown() {
    if (array.length < 3) {
      throw new Error(`Check failed, array length ${array.length}`);
    }

    for (const klass of array) {
      const instance = new klass();
      if (!instance.check())
        throw new Error(`instance.check() failed`);
    }
  }

  const DETERMINISTIC_RUNS = 1;
  const LOCAL_ITERATIONS = 100;

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

CreateBenchmark("EvaluateSinglePublicFieldClass", optimize);
CreateBenchmark("EvaluateMultiPublicFieldClass", optimize);
CreateBenchmark("EvaluateSinglePrivateFieldClass", optimize);
CreateBenchmark("EvaluateMultiPrivateFieldClass", optimize);
CreateBenchmark("EvaluateSinglePrivateMethodClass", optimize);
CreateBenchmark("EvaluateMultiPrivateMethodClass", optimize);
CreateBenchmark("EvaluateSingleComputedFieldClass", optimize);
CreateBenchmark("EvaluateMultiComputedFieldClass", optimize);
