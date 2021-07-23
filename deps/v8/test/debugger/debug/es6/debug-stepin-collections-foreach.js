// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

Debug = debug.Debug

var exception = null;

function listener(event, exec_state, event_data, data) {
  try {
    if (event == Debug.DebugEvent.Break) {
      exec_state.prepareStep(Debug.StepAction.StepInto);
      print(event_data.sourceLineText());
      assertTrue(
          event_data.sourceLineText().indexOf(`B${breaks++}`) > 0);
    }
  } catch (e) {
    print(e);
    quit();
    exception = e;
  }
}

function cb_set(num) {
  print("element " + num);  // B2 B5 B8
  return true;              // B3 B6 B9
}                           // B4 B7 B10

function cb_map(key, val) {
  print("key " + key + ", value " + val);  // B2 B5 B8
  return true;                             // B3 B6 B9
}                                          // B4 B7 B10

var s = new Set();
s.add(1);
s.add(2);
s.add(3);

var m = new Map();
m.set('foo', 1);
m.set('bar', 2);
m.set('baz', 3);

var breaks = 0;
Debug.setListener(listener);
debugger;                 // B0
s.forEach(cb_set);        // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

breaks = 0;
Debug.setListener(listener);
debugger;                 // B0
m.forEach(cb_map);        // B1
Debug.setListener(null);  // B11
assertNull(exception);
assertEquals(12, breaks);

// Test two levels of builtin callbacks:
// Array.forEach calls a callback function, which by itself uses
// Array.forEach with another callback function.

function cb_set_2(num) {
  print("element " + num);  // B3 B6 B9  B15 B18 B21 B27 B30 B33
  return true;              // B4 B7 B10 B16 B19 B22 B28 B31 B34
}                           // B5 B8 B11 B17 B20 B23 B29 B32 B35

function cb_map_2(k, v) {
  print(`key ${k}, value ${v}`);  // B3 B6 B9  B15 B18 B21 B27 B30 B33
  return true;                    // B4 B7 B10 B16 B19 B22 B28 B31 B34
}                                 // B5 B8 B11 B17 B20 B23 B29 B32 B35

function cb_set_foreach(num) {
  s.forEach(cb_set_2);      // B2  B14 B26
  print("back.");           // B12 B24 B36
}                           // B13 B25 B37

function cb_map_foreach(key, val) {
  m.forEach(cb_map_2);      // B2  B14 B26
  print("back.");           // B12 B24 B36
}                           // B13 B25 B37

breaks = 0;
Debug.setListener(listener);
debugger;                   // B0
s.forEach(cb_set_foreach);  // B1
Debug.setListener(null);    // B38
assertNull(exception);
assertEquals(39, breaks);

breaks = 0;
Debug.setListener(listener);
debugger;                   // B0
m.forEach(cb_map_foreach);  // B1
Debug.setListener(null);    // B38
assertNull(exception);
assertEquals(39, breaks);
