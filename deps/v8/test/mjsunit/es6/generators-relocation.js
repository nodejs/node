// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

var Debug = debug.Debug;

function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}

function RunTest(formals_and_body, args, value1, value2) {
  // A null listener. It isn't important what the listener does.
  function listener(event, exec_state, event_data, data) {
  }

  // Create the generator function outside a debugging context. It will probably
  // be lazily compiled.
  var gen = (function*(){}).constructor.apply(null, formals_and_body);

  // Instantiate the generator object.
  var obj = gen.apply(null, args);

  // Advance to the first yield.
  assertIteratorResult(value1, false, obj.next());

  // Enable the debugger, which should force recompilation of the generator
  // function and relocation of the suspended generator activation.
  Debug.setListener(listener);

  // Add a breakpoint on line 3 (the second yield).
  var bp = Debug.setBreakPoint(gen, 3);

  // Check that the generator resumes and suspends properly.
  assertIteratorResult(value2, false, obj.next());

  // Disable debugger -- should not force recompilation.
  Debug.clearBreakPoint(bp);
  Debug.setListener(null);

  // Run to completion.
  assertIteratorResult(undefined, true, obj.next());
}

function prog(a, b, c) {
  return a + ';\n' + 'yield ' + b + ';\n' + 'yield ' + c;
}

// Simple empty local scope.
RunTest([prog('', '1', '2')], [], 1, 2);

RunTest([prog('for (;;) break', '1', '2')], [], 1, 2);

RunTest([prog('while (0) foo()', '1', '2')], [], 1, 2);

RunTest(['a', prog('var x = 3', 'a', 'x')], [1], 1, 3);

RunTest(['a', prog('', '1', '2')], [42], 1, 2);

RunTest(['a', prog('for (;;) break', '1', '2')], [42], 1, 2);
