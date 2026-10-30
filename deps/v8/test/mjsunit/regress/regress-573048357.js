// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// A Baseline constructor called from a Maglev frame with an inlined construct
// stub must keep its constructor flag in the captured stack trace.

function captureFrames() {
  Error.prepareStackTrace = (e, frames) =>
      frames.map(f => [f.getFunctionName(), f.isConstructor()]);
  const frames = new Error().stack;
  Error.prepareStackTrace = undefined;
  return frames;
}
%NeverOptimizeFunction(captureFrames);

(function TestConstructorCapturesStack() {
  function C(capture) {
    if (capture) {
      this.stack = new Error().stack;
      this.frames = captureFrames();
    }
  }

  function constructC(capture) {
    return new C(capture);
  }

  %NeverOptimizeFunction(C);
  if (%IsSparkplugEnabled()) %CompileBaseline(C);
  %PrepareFunctionForOptimization(constructC);
  constructC(false);
  %OptimizeMaglevOnNextCall(constructC);

  const res = constructC(true);
  assertMatches(/at new C/, res.stack);
  assertEquals(['captureFrames', false], res.frames[0]);
  assertEquals(['C', true], res.frames[1]);
  assertEquals(['constructC', false], res.frames[2]);
})();

(function TestConstructorCallsCapturingFunction() {
  function D() {
    return captureFrames();
  }

  function C(capture) {
    if (capture) this.frames = D();
  }

  function constructC(capture) {
    return new C(capture);
  }

  %NeverOptimizeFunction(D);
  %NeverOptimizeFunction(C);
  if (%IsSparkplugEnabled()) {
    %CompileBaseline(D);
    %CompileBaseline(C);
  }
  %PrepareFunctionForOptimization(constructC);
  constructC(false);
  %OptimizeMaglevOnNextCall(constructC);

  const res = constructC(true);
  assertEquals(['captureFrames', false], res.frames[0]);
  assertEquals(['D', false], res.frames[1]);
  assertEquals(['C', true], res.frames[2]);
  assertEquals(['constructC', false], res.frames[3]);
})();
