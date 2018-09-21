// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug

var exception = null;
var log = [];

function listener(event, exec_state, event_data, data) {
  if (event != Debug.DebugEvent.Break) return;
  try {
    print(event_data.sourceLineText());
    var entry = "";
    for (var i = 0; i < exec_state.frameCount(); i++) {
      entry += exec_state.frame(i).sourceLineText().substr(-1);
      entry += exec_state.frame(i).sourceColumn();
    }
    log.push(entry);
    exec_state.prepareStep(Debug.StepAction.StepIn);
  } catch (e) {
    exception = e;
  }
};

var target = {};
var handler = {
  has: function(target, name) {
    return true;                     // h
  },                                 // i
  get: function(target, name) {
    return 42;                       // j
  },                                 // k
  set: function(target, name, value) {
    return false;                    // l
  },                                 // m
}

var proxy = new Proxy(target, handler);

Debug.setListener(listener);
debugger;                            // a
var has = "step" in proxy;           // b
var get = proxy.step;                // c
proxy.step = 43;                     // d

Debug.setListener(null);             // g

assertNull(exception);
assertTrue(has);
assertEquals(42, get);

assertEquals([
  "a0",
  "b10", "h4b17", "h16b17", // [[Has]]
  "c10", "j4c16", "j14c16", // [[Get]]
  "d0", "l4d11", "l17d11",  // [[Set]]
  "g0"
], log);
