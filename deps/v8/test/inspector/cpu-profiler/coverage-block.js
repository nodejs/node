// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan --turbofan
// Flags: --no-stress-flush-code
// Flags: --no-stress-incremental-marking
// Flags: --no-concurrent-recompilation
// Flags: --no-maglev

var source =
`
function fib(x) {
  if (x < 2) return 1;
  return fib(x-1) + fib(x-2);
}
function is_optimized(f) {
  return (%GetOptimizationStatus(f) & 16) ? "optimized" : "unoptimized";
}
(function iife() {
  return 1;
})();
fib(5);
`;

var break_source =
`
function g() {
  debugger;
}
function f(x) {
  if (x == 0) g();
  else f(x - 1);
}
function h() {
  g();
}
f(3);
`;

var nested =
`
var f = (function outer() {
  function nested_0() {
    return function nested_1() {
      return function nested_2() {
        return function nested_3() {}
      }
    }
  }
  function nested_4() {}
  return nested_0();
})();
f()()();
`;

let {session, contextGroup, Protocol} = InspectorTest.start("Test collecting code coverage data with Profiler.collectCoverage.");

function ClearAndGC() {
  return Protocol.Runtime.evaluate({ expression: "fib = g = f = h = is_optimized = null;" })
             .then(GC);
}

function GC() {
  return Protocol.HeapProfiler.collectGarbage();
}

function LogSorted(message) {
  message.result.result.sort((a, b) => parseInt(a.scriptId) - parseInt(b.scriptId));
  return InspectorTest.logMessage(message);
}

InspectorTest.runTestSuite([
  function testPreciseCountBaseline(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(GC)
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true}))
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testPreciseCountCoverage(next)
  {
    Protocol.Runtime.enable()
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true}))
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(ClearAndGC)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testPreciseCountCoverageIncremental(next)
  {
    Protocol.Runtime.enable()
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true}))
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(() => Protocol.Runtime.evaluate({ expression: "is_optimized(fib)" }))
      .then(message => InspectorTest.logMessage(message))
      .then(() => Protocol.Runtime.evaluate({ expression: "fib(20)" }))
      .then(message => InspectorTest.logMessage(message))
      .then(() => Protocol.Runtime.evaluate({ expression: "is_optimized(fib)" }))
      .then(message => InspectorTest.logMessage(message))
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testPreciseCoverageFail(next)
  {
    Protocol.Runtime.enable()
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(ClearAndGC)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(InspectorTest.logMessage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testBestEffortCoverage(next)
  {
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(ClearAndGC)
      .then(Protocol.Profiler.getBestEffortCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.getBestEffortCoverage)
      .then(LogSorted)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testBestEffortCoverageWithPreciseBinaryEnabled(next)
  {
    Protocol.Runtime.enable()
    .then(Protocol.Profiler.enable)
    .then(() => Protocol.Profiler.startPreciseCoverage({detailed: true}))
    .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
    .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
    .then(InspectorTest.logMessage)
    .then(ClearAndGC)
    .then(Protocol.Profiler.getBestEffortCoverage)
    .then(LogSorted)
    .then(Protocol.Profiler.getBestEffortCoverage)
    .then(LogSorted)
    .then(ClearAndGC)
    .then(Protocol.Profiler.stopPreciseCoverage)
    .then(Protocol.Profiler.disable)
    .then(Protocol.Runtime.disable)
    .then(ClearAndGC)
    .then(next);
  },
  function testBestEffortCoverageWithPreciseCountEnabled(next)
  {
    Protocol.Runtime.enable()
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true}))
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(ClearAndGC)
      .then(Protocol.Profiler.getBestEffortCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.getBestEffortCoverage)
      .then(LogSorted)
      .then(ClearAndGC)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testEnablePreciseCountCoverageAtPause(next)
  {
    function handleDebuggerPause() {
      Protocol.Profiler.enable()
          .then(() => Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true}))
          .then(Protocol.Debugger.resume)
    }
    Protocol.Debugger.enable();
    Protocol.Debugger.oncePaused().then(handleDebuggerPause);
    Protocol.Runtime.enable()
      .then(() => Protocol.Runtime.compileScript({ expression: break_source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(ClearAndGC)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(ClearAndGC)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(Protocol.Debugger.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testPreciseBinaryCoverage(next)
  {
    Protocol.Runtime.enable()
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({detailed: true}))
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(() => Protocol.Runtime.evaluate({ expression: "is_optimized(fib)" }))
      .then(message => InspectorTest.logMessage(message))
      .then(() => Protocol.Runtime.evaluate({ expression: "fib(20)" }))
      .then(message => InspectorTest.logMessage(message))
      .then(() => Protocol.Runtime.evaluate({ expression: "is_optimized(fib)" }))
      .then(message => InspectorTest.logMessage(message))
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testPreciseEmptyScriptCoverageEntries(next)
  {
    // Enabling the debugger holds onto script objects even though its
    // functions can be garbage collected. We would get empty ScriptCoverage
    // entires unless we remove them.
    Protocol.Debugger.enable()
      .then(Protocol.Runtime.enable)
      .then(() => Protocol.Runtime.compileScript({ expression: source, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(ClearAndGC)
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({detailed: true}))
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(Protocol.Debugger.disable)
      .then(ClearAndGC)
      .then(next);
  },
  function testPreciseCountCoveragePartial(next)
  {
    Protocol.Runtime.enable()
      .then(Protocol.Profiler.enable)
      .then(() => Protocol.Profiler.startPreciseCoverage({callCount: true, detailed: true}))
      .then(() => Protocol.Runtime.compileScript({ expression: nested, sourceURL: arguments.callee.name, persistScript: true }))
      .then((result) => Protocol.Runtime.runScript({ scriptId: result.result.scriptId }))
      .then(InspectorTest.logMessage)
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(() => Protocol.Runtime.evaluate({ expression: "f()" }))
      .then(Protocol.Profiler.takePreciseCoverage)
      .then(LogSorted)
      .then(Protocol.Profiler.stopPreciseCoverage)
      .then(Protocol.Profiler.disable)
      .then(Protocol.Runtime.disable)
      .then(ClearAndGC)
      .then(next);
  },
]);
