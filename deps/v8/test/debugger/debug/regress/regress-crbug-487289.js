// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var Debug = debug.Debug;
var receiver = null;

Debug.setListener(function(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  receiver = exec_state.frame(0).evaluate('this').value();
});

function f() { debugger; }

var expected = {};
f.call(expected);

assertEquals(expected, receiver);
