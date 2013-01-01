// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-debug-as debug

// Get the Debug object exposed from the debug context global object.
var Debug = debug.Debug;

// Accepts a function/closure 'fun' that must have a debugger statement inside.
// A variable 'variable_name' must be initialized before debugger statement
// and returned after the statement. The test will alter variable value when
// on debugger statement and check that returned value reflects the change.
function RunPauseTest(scope_number, variable_name, expected_new_value, fun) {
  var old_value = fun();

  var listener_delegate;
  var listener_called = false;
  var exception = null;

  function listener_delegate(exec_state) {
    var scope = exec_state.frame(0).scope(scope_number);
    scope.setVariableValue(variable_name, expected_new_value);
  }

  function listener(event, exec_state, event_data, data) {
    try {
      if (event == Debug.DebugEvent.Break) {
        listener_called = true;
        listener_delegate(exec_state);
      }
    } catch (e) {
      exception = e;
    }
  }

  // Add the debug event listener.
  Debug.setListener(listener);

  var actual_new_value;
  try {
    actual_new_value = fun();
  } finally {
    Debug.setListener(null);
  }

  if (exception != null) {
   assertUnreachable("Exception: " + exception);
  }
  assertTrue(listener_called);

  assertTrue(old_value != actual_new_value);
  assertTrue(expected_new_value == actual_new_value);
}

// Accepts a closure 'fun' that returns a variable from it's outer scope.
// The test changes the value of variable via the handle to function and checks
// that the return value changed accordingly.
function RunClosureTest(scope_number, variable_name, expected_new_value, fun) {
  var old_value = fun();

  var fun_mirror = Debug.MakeMirror(fun);

  var scope = fun_mirror.scope(scope_number);
  scope.setVariableValue(variable_name, expected_new_value);

  var actual_new_value = fun();

  assertTrue(old_value != actual_new_value);
  assertTrue(expected_new_value == actual_new_value);
}

// Test changing variable value when in pause
RunPauseTest(1, 'v1', 5, (function Factory() {
  var v1 = 'cat';
  return function() {
    debugger;
    return v1;
  }
})());

RunPauseTest(1, 'v2', 11, (function Factory(v2) {
  return function() {
    debugger;
    return v2;
  }
})('dog'));

RunPauseTest(3, 'foo', 77, (function Factory() {
  var foo = "capybara";
  return (function() {
    var bar = "fish";
    try {
      throw {name: "test exception"};
    } catch (e) {
      return function() {
        debugger;
        bar = "beast";
        return foo;
      }
    }
  })();
})());



// Test changing variable value in closure by handle
RunClosureTest(0, 'v1', 5, (function Factory() {
  var v1 = 'cat';
  return function() {
    return v1;
  }
})());

RunClosureTest(0, 'v2', 11, (function Factory(v2) {
  return function() {
    return v2;
  }
})('dog'));

RunClosureTest(2, 'foo', 77, (function Factory() {
  var foo = "capybara";
  return (function() {
    var bar = "fish";
    try {
      throw {name: "test exception"};
    } catch (e) {
      return function() {
        bar = "beast";
        return foo;
      }
    }
  })();
})());


// Test value description protocol JSON
assertEquals(true, Debug.TestApi.CommandProcessorResolveValue({value: true}));

assertSame(null, Debug.TestApi.CommandProcessorResolveValue({type: "null"}));
assertSame(undefined,
    Debug.TestApi.CommandProcessorResolveValue({type: "undefined"}));

assertSame("123", Debug.TestApi.CommandProcessorResolveValue(
    {type: "string", stringDescription: "123"}));
assertSame(123, Debug.TestApi.CommandProcessorResolveValue(
    {type: "number", stringDescription: "123"}));

assertSame(Number, Debug.TestApi.CommandProcessorResolveValue(
    {handle: Debug.MakeMirror(Number).handle()}));
assertSame(RunClosureTest, Debug.TestApi.CommandProcessorResolveValue(
    {handle: Debug.MakeMirror(RunClosureTest).handle()}));

