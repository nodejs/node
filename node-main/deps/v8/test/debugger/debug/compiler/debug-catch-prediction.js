// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Test debug event catch prediction for thrown exceptions. We distinguish
// between "caught" and "uncaught" based on the following assumptions:
//  1) try-catch   : Will always catch the exception.
//  2) try-finally : Will always re-throw the exception.

Debug = debug.Debug;

var log = [];

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Exception) {
      log.push([event_data.exception(), event_data.uncaught()]);
    }
  } catch (e) {
    %AbortJS(e + "\n" + e.stack);
  }
}

Debug.setBreakOnException();
Debug.setListener(listener);

(function TryCatch() {
  log = [];  // Clear log.
  function f(a) {
    try {
      throw "boom" + a;
    } catch(e) {
      return e;
    }
  }
  %PrepareFunctionForOptimization(f);
  assertEquals("boom1", f(1));
  assertEquals("boom2", f(2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("boom3", f(3));
  print("Collect log:", log);
  assertEquals([["boom1",false], ["boom2",false], ["boom3",false]], log);
})();

(function TryFinally() {
  log = [];  // Clear log.
  function f(a) {
    try {
      throw "baem" + a;
    } finally {
      return a + 10;
    }
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(11, f(1));
  assertEquals(12, f(2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(13, f(3));
  print("Collect log:", log);
  assertEquals([["baem1",true], ["baem2",true], ["baem3",true]], log);
})();

(function TryCatchFinally() {
  log = [];  // Clear log.
  function f(a) {
    try {
      throw "wosh" + a;
    } catch(e) {
      return e + a;
    } finally {
      // Nothing.
    }
  }
  %PrepareFunctionForOptimization(f);
  assertEquals("wosh11", f(1));
  assertEquals("wosh22", f(2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("wosh33", f(3));
  print("Collect log:", log);
  assertEquals([["wosh1",false], ["wosh2",false], ["wosh3",false]], log);
})();

(function TryCatchNestedFinally() {
  log = [];  // Clear log.
  function f(a) {
    try {
      try {
        throw "bang" + a;
      } finally {
        // Nothing.
      }
    } catch(e) {
      return e + a;
    }
  }
  %PrepareFunctionForOptimization(f);
  assertEquals("bang11", f(1));
  assertEquals("bang22", f(2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals("bang33", f(3));
  print("Collect log:", log);
  assertEquals([["bang1",false], ["bang2",false], ["bang3",false]], log);
})();

(function TryFinallyNestedCatch() {
  log = [];  // Clear log.
  function f(a) {
    try {
      try {
        throw "peng" + a;
      } catch(e) {
        return e
      }
    } finally {
      return a + 10;
    }
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(11, f(1));
  assertEquals(12, f(2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(13, f(3));
  print("Collect log:", log);
  assertEquals([["peng1",false], ["peng2",false], ["peng3",false]], log);
})();

(function TryFinallyNestedFinally() {
  log = [];  // Clear log.
  function f(a) {
    try {
      try {
        throw "oops" + a;
      } finally {
        // Nothing.
      }
    } finally {
      return a + 10;
    }
  }
  %PrepareFunctionForOptimization(f);
  assertEquals(11, f(1));
  assertEquals(12, f(2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(13, f(3));
  print("Collect log:", log);
  assertEquals([["oops1",true], ["oops2",true], ["oops3",true]], log);
})();
