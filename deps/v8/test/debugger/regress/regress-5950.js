// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f() {
  debugger;
}

function g() {
  f();
}

function h() {
  'use strict';
  return g();
}

h();
h();

var Debug = debug.Debug;
var exception = null;

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    var all_scopes = exec_state.frame().allScopes();
  } catch (e) {
    exception = e;
  }
}

Debug.setListener(listener);
%OptimizeFunctionOnNextCall(h);
h();
Debug.setListener(null);
assertNull(exception);
