// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';

var Debug = debug.Debug
var done, stepCount;
var exception = null;

function listener(event, execState, eventData, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    if (!done) {
      execState.prepareStep(Debug.StepAction.StepIn);
      var s = execState.frame().sourceLineText();
      assertTrue(s.indexOf('// ' + stepCount + '.') !== -1);
      stepCount++;
    }
  } catch (e) {
    exception = e;
  }
};

Debug.setListener(listener);


class Base {
  constructor() {
    var x = 1;    // 1.
    var y = 2;    // 2.
    done = true;  // 3.
  }
}

class Derived extends Base {}  // 0.


(function TestBreakPointInConstructor() {
  done = false;
  stepCount = 1;
  var bp = Debug.setBreakPoint(Base, 0);

  new Base();
  assertEquals(4, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestDefaultConstructor() {
  done = false;
  stepCount = 1;

  var bp = Debug.setBreakPoint(Base, 0);
  new Derived();
  assertEquals(4, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestStepInto() {
  done = false;
  stepCount = -1;

  function f() {
    new Derived();  // -1.
  }

  var bp = Debug.setBreakPoint(f, 0);
  f();
  assertEquals(4, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestExtraIndirection() {
  done = false;
  stepCount = -2;

  class Derived2 extends Derived {}  // -1.

  function f() {
    new Derived2();  // -2.
  }

  var bp = Debug.setBreakPoint(f, 0);
  f();
  assertEquals(4, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestBoundClass() {
  done = false;
  stepCount = -1;

  var bound = Derived.bind(null);

  function f() {
    new bound();  // -1.
  }

  var bp = Debug.setBreakPoint(f, 0);
  f();
  assertEquals(4, stepCount);

  Debug.clearBreakPoint(bp);
})();


Debug.setListener(null);
assertNull(exception);
