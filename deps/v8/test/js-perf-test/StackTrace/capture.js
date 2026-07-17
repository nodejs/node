// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

function Simple() {
  new Error("Simple Error");
}

class CustomError extends Error {};
function Custom() {
  new CustomError("Custom Error");
}

function Inline() {
  function Inner() {
    new Error("Error from inlined function!");
  }
  function Middle() { Inner(); }
  function Outer() { Middle(); }

  %PrepareFunctionForOptimization(Outer);
  Outer();
  Outer();
  %OptimizeFunctionOnNextCall(Outer);
  Outer();
}

const kInitialRecursionValue = 10;
function Recursive() {
  function StepOne(val) {
    if (val <= 0) return new Error("Error in StepOne!");
    StepTwo(val - 3);
    StepTwo(val - 4);
  }
  function StepTwo(val) {
    if (val <= 0) return new Error("Error in StepTwo!");
    StepOne(val - 1);
    StepOne(val - 2);
  }

  StepOne(kInitialRecursionValue);
}

createSuite('Simple-Capture-Error', 1000, Simple, () => {});
createSuite('Custom-Capture-Error', 1000, Custom, () => {});

createSuite('Inline-Capture-Error', 1000, Inline, () => {});
createSuite('Recursive-Capture-Error', 1000, Recursive, () => {});

})();
