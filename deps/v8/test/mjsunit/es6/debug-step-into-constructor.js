// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

'use strict';

var Debug = debug.Debug
var done, stepCount;

function listener(event, execState, eventData, data) {
  if (event == Debug.DebugEvent.Break) {
    if (!done) {
      execState.prepareStep(Debug.StepAction.StepIn);
      var s = execState.frame().sourceLineText();
      assertTrue(s.indexOf('// ' + stepCount + '.') !== -1);
      stepCount++;
    }
  }
};

Debug.setListener(listener);


class Base {
  constructor() { // 1.
    var x = 1;    // 2.
    var y = 2;    // 3.
    done = true;  // 4.
  }
}

class Derived extends Base {}


(function TestBreakPointInConstructor() {
  done = false;
  stepCount = 1;
  var bp = Debug.setBreakPoint(Base, 0);

  new Base();
  assertEquals(1, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestDefaultConstructor() {
  done = false;
  stepCount = 1;

  var bp = Debug.setBreakPoint(Base, 0);
  new Derived();
  assertEquals(1, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestStepInto() {
  done = false;
  stepCount = 0;

  function f() {
    new Derived();  // 0.
  }

  var bp = Debug.setBreakPoint(f, 0);
  f();
  assertEquals(1, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestExtraIndirection() {
  done = false;
  stepCount = 0;

  class Derived2 extends Derived {}

  function f() {
    new Derived2();  // 0.
  }

  var bp = Debug.setBreakPoint(f, 0);
  f();
  assertEquals(1, stepCount);

  Debug.clearBreakPoint(bp);
})();


(function TestBoundClass() {
  done = false;
  stepCount = 0;

  var bound = Derived.bind(null);

  function f() {
    new bound();  // 0.
  }

  var bp = Debug.setBreakPoint(f, 0);
  f();
  assertEquals(1, stepCount);

  Debug.clearBreakPoint(bp);
})();


Debug.setListener(null);
