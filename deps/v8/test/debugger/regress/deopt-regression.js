// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug

var foo = {
  [Symbol.toPrimitive]() {return "xyz";}
}

function f() {
  var v = foo;
  var o = {[v]: v};
  return o["xyz"];
};

Debug = debug.Debug

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
  }
};

// Add the debug event listener.
Debug.setListener(listener);

%DebugPrint(f());
%CompileBaseline(f);
%DebugPrint(f());

foo[Symbol.toPrimitive] = function(hint) {
  print("Setting breakpoint...");
  const breakid = Debug.setBreakPoint(f, 3);
  return "xyz";
};

%DebugPrint(f());

// Get rid of the debug event listener.
Debug.setListener(null);
