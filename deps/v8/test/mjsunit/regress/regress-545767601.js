// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function onFinally() {
  leakedFinally = onFinally.caller;
}
function attachFinally(promise) {
  return promise.finally(onFinally);
}
async function trigger() {
  %PrepareFunctionForOptimization(attachFinally);
  attachFinally(Promise.resolve());
  %OptimizeFunctionOnNextCall(attachFinally);
  attachFinally(Promise.resolve());
  await Promise.resolve();
  const objectResult = {field: 0x41414141};
  let returnPrimitive = false;
  Promise.prototype.then = function() {
    return returnPrimitive ? 1 : objectResult;
  };
  function readConstructedField() {
    new leakedFinally().field;
  }
  %PrepareFunctionForOptimization(readConstructedField);
  readConstructedField();
  %OptimizeFunctionOnNextCall(readConstructedField);
  returnPrimitive = true;
  readConstructedField();
}

assertThrowsAsync(trigger(), TypeError);
