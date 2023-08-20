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


// Test stepping into callbacks passed to builtin functions.

Debug = debug.Debug

var exception = null;

function array_listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      print(event_data.sourceLineText(), breaks);
      assertTrue(event_data.sourceLineText().indexOf(`B${breaks++}`) > 0);
      exec_state.prepareStep(Debug.StepAction.StepInto);
    }
  } catch (e) {
    print(e);
    quit();
    exception = e;
  }
};

function cb_false(num) {
  print("element " + num);  // B2 B5 B8
  return false;             // B3 B6 B9
}                           // B4 B7 B10

function cb_true(num) {
  print("element " + num);  // B2 B5 B8
  return true;              // B3 B6 B9
}                           // B4 B7 B10

function cb_reduce(a, b) {
  print("elements " + a + " and " + b);  // B2 B5
  return a + b;                          // B3 B6
}                                        // B4 B7

var a = [1, 2, 3];

var breaks = 0;
Debug.setListener(array_listener);
debugger;                 // B0
a.forEach(cb_true);       // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

breaks = 0;
Debug.setListener(array_listener);
debugger;                 // B0
a.some(cb_false);         // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

breaks = 0;
Debug.setListener(array_listener);
debugger;                 // B0
a.every(cb_true);         // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

breaks = 0;
Debug.setListener(array_listener);
debugger;                 // B0
a.map(cb_true);           // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

breaks = 0;
Debug.setListener(array_listener);
debugger;                 // B0
a.filter(cb_true);        // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

breaks = 0;
Debug.setListener(array_listener);
debugger;                 // B0
a.reduce(cb_reduce);      // B1
Debug.setListener(null);  // B8
assertNull(exception);
assertEquals(9, breaks);

breaks = 0;
Debug.setListener(array_listener);
debugger;                  // B0
a.reduceRight(cb_reduce);  // B1
Debug.setListener(null);   // B8
assertNull(exception);
assertEquals(9, breaks);


// Test two levels of builtin callbacks:
// Array.forEach calls a callback function, which by itself uses
// Array.forEach with another callback function.

function cb_true_2(num) {
  print("element " + num);  // B3 B6 B9  B15 B18 B21 B27 B30 B33
  return true;              // B4 B7 B10 B16 B19 B22 B28 B31 B34
}                           // B5 B8 B11 B17 B20 B23 B29 B32 B35

function cb_foreach(num) {
  a.forEach(cb_true_2);     // B2  B14 B20 B26
  print("back.");           // B12 B18 B24 B36
}                           // B13 B19 B25 B37

breaks = 0;
Debug.setListener(array_listener);
debugger;                   // B0
a.forEach(cb_foreach);      // B1
Debug.setListener(null);    // B38
assertNull(exception);
assertEquals(39, breaks);
