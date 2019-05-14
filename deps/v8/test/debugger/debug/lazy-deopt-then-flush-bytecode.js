// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --noalways-opt --stress-flush-bytecode
// Flags: --expose-gc

Debug = debug.Debug

function foo() {
  return 44;
}

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;

  // Optimize foo.
  %OptimizeFunctionOnNextCall(foo);
  foo();
  assertOptimized(foo);

  // Lazily deopt foo, which marks the code for deoptimization and invalidates
  // the DeoptimizationData, but doesn't unlink the optimized code entry in
  // foo's JSFunction.
  %DeoptimizeFunction(foo);

  // Run the GC. Since the DeoptimizationData is now dead, the bytecode
  // associated with the optimized code is free to be flushed, which also
  // free's the feedback vector meta-data.
  gc();

  // Execute foo with side-effect checks, which causes the debugger to call
  // DeoptimizeFunction on foo. Even though the code is already marked for
  // deoptimization, this will try to unlink the optimized code from the
  // feedback vector, which will fail due to the feedback meta-data being
  // flushed. The deoptimizer should call JSFunction::ResetIfBytecodeFlushed
  // before trying to do this, which will clear the whole feedback vector and
  // reset the JSFunction's code entry field to CompileLazy.
  exec_state.frame(0).evaluate("foo()", true);
}

// Add the debug event listener.
Debug.setListener(listener);

function f() {
  debugger;
}
f();
