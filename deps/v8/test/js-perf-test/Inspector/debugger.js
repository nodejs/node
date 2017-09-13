// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  new BenchmarkSuite('Debugger.paused', [10000], [
    new Benchmark('Debugger.paused', false, false, 0, DebuggerPaused, Setup, TearDown),
  ]);

  function Setup() {
    SendMessage('Debugger.enable');
    // Force lazy compilation of inspector related scripts.
    SendMessage('Runtime.evaluate', {expression: ''});
  }

  function TearDown() {
    SendMessage('Debugger.disable');
  }

  function DebuggerPaused() {
    for (var i = 0; i < 10; ++i) {
      debugger;
    }
  }
})();
