// Copyright 2011 the V8 project authors. All rights reserved.
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

Debug = debug.Debug;

var test_name;
var listener_delegate;
var listener_called;
var exception;
var expected_receiver;
var begin_test_count = 0;
var end_test_count = 0;
var break_count = 0;

// Debug event listener which delegates. Exceptions have to be
// explicitly caught here and checked later because exception in the
// listener are not propagated to the surrounding JavaScript code.
function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      break_count++;
      listener_called = true;
      listener_delegate(exec_state);
    }
  } catch (e) {
    exception = e;
  }
}

// Add the debug event listener.
Debug.setListener(listener);


// Initialize for a new test.
function BeginTest(name) {
  test_name = name;
  listener_called = false;
  exception = null;
  begin_test_count++;
}


// Check result of a test.
function EndTest() {
  assertTrue(listener_called, "listener not called for " + test_name);
  assertNull(exception, test_name);
  end_test_count++;
}


// Check that the debugger correctly reflects that the receiver is not
// converted to object for strict mode functions.
function Strict() { "use strict"; debugger; }
function TestStrict(receiver) {
  expected_receiver = receiver;
  Strict.call(receiver);
}

listener_delegate = function(exec_state) {
  var receiver = exec_state.frame().receiver();
  assertEquals(expected_receiver, receiver.value())
}

BeginTest("strict: undefined"); TestStrict(undefined); EndTest();
BeginTest("strict: null"); TestStrict(null); EndTest();
BeginTest("strict: 1"); TestStrict(1); EndTest();
BeginTest("strict: 1.2"); TestStrict(1.2); EndTest();
BeginTest("strict: 'asdf'"); TestStrict('asdf'); EndTest();
BeginTest("strict: true"); TestStrict(true); EndTest();


// Check that the debugger correctly reflects the object conversion of
// the receiver for non-strict mode functions.
function NonStrict() { debugger; }
function TestNonStrict(receiver) {
  // null and undefined should be transformed to the global object and
  // primitives should be wrapped.
  expected_receiver = (receiver == null) ? this : Object(receiver);
  NonStrict.call(receiver);
}

listener_delegate = function(exec_state) {
  var receiver = exec_state.frame().receiver();
  assertEquals(expected_receiver, receiver.value());
}

BeginTest("non-strict: undefined"); TestNonStrict(undefined); EndTest();
BeginTest("non-strict: null"); TestNonStrict(null); EndTest();
BeginTest("non-strict: 1"); TestNonStrict(1); EndTest();
BeginTest("non-strict: 1.2"); TestNonStrict(1.2); EndTest();
BeginTest("non-strict: 'asdf'"); TestNonStrict('asdf'); EndTest();
BeginTest("non-strict: true"); TestNonStrict(true); EndTest();


assertEquals(begin_test_count, break_count,
             'one or more tests did not enter the debugger');
assertEquals(begin_test_count, end_test_count,
             'one or more tests did not have its result checked');
