// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

const kErrorCount = 100000;
let errorsCreatedBySetup;

function CreateErrors(fn) {
  counter = 0;
  errorsCreatedBySetup = [];
  for (let i = 0; i < kErrorCount; ++i) {
    errorsCreatedBySetup[i] = fn();
  }
}

function SimpleSetup() {
  CreateErrors(() => new Error("Simple Error"));
}

class CustomError extends Error {};
function CustomSetup() {
  CreateErrors(() => new CustomError("Custom Error"));
}

function InlineSetup() {
  function Inner() {
    return new Error("Throwing from inlined function!");
  }
  function Middle() { return Inner(); }
  function Outer() { return Middle(); }

  Outer();
  Outer();
  %OptimizeFunctionOnNextCall(Outer);
  Outer();

  CreateErrors(() => Outer());
}

const kInitialRecursionValue = 12;
function RecursiveSetup() {
  counter = 0;
  errorsCreatedBySetup = [];
  function StepOne(val) {
    if (val <= 0) {
      errorsCreatedBySetup.push(new Error("Error in StepOne!"));
      return;
    }
    StepTwo(val - 3);
    StepTwo(val - 4);
  }
  function StepTwo(val) {
    if (val <= 0) {
      errorsCreatedBySetup.push(new Error("Error in StepTwo!"));
      return;
    }
    StepOne(val - 1);
    StepOne(val - 2);
  }

  while (errorsCreatedBySetup.length < kErrorCount) {
    StepOne(kInitialRecursionValue);
  }
}

let counter;
function SerializeStack() {
  if (counter < errorsCreatedBySetup.length) {
    // Trigger serialization by accessing Error.stack.
    %FlattenString(errorsCreatedBySetup[counter++].stack);
  } else {
    // All errors are serialized. The stack trace string is now cached, so
    // re-iterating the array is a simple property lookup. Instead,
    // a simple Error object is created and serialized, otherwise the benchmark
    // result would fluctuate heavily if it reaches the end.
    %FlattenString(new Error().stack);
  }
}

createSuite('Simple-Serialize-Error.stack', 1000, SerializeStack, SimpleSetup);
createSuite('Custom-Serialize-Error.stack', 1000, SerializeStack, CustomSetup);

createSuite('Inline-Serialize-Error.stack', 1000, SerializeStack, InlineSetup);
createSuite('Recursive-Serialize-Error.stack', 1000, SerializeStack, RecursiveSetup);

})();
