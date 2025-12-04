// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  new BenchmarkSuite('Runtime.evaluate(String16Cstor)', [1000], [
    new Benchmark('Runtime.evaluate(String16Cstor)', false, false, 0, EvaluateTest, Setup, TearDown),
  ]);

  function Setup() {
    // Force lazy compilation of inspector related scripts.
    SendMessage('Runtime.evaluate', {expression: ''});
  }

  function TearDown() {
  }

  function EvaluateTest() {
    // This is meant to exercise the overhead of v8_inspector::String16
    // constructors. https://crbug.com/738469
    for (var i = 0; i < 10; ++i) {
      SendMessage('Runtime.evaluate', {expression: '({})', returnByValue: true});
    }
  }
})();
