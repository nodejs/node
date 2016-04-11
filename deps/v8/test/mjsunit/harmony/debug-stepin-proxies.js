// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug --harmony-proxies

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
  enumerate: function(target) {
    function* keys() {               // n
      yield "foo";                   // o
      yield "bar";                   // p
    }                                // q
    return keys();                   // r
  },                                 // s
}

var proxy = new Proxy(target, handler);

Debug.setListener(listener);
debugger;                            // a
var has = "step" in proxy;           // b
var get = proxy.step;                // c
proxy.step = 43;                     // d
for (var i in proxy) {               // e
  log.push(i);                       // f
}

Debug.setListener(null);             // g

assertNull(exception);
assertTrue(has);
assertEquals(42, get);

assertEquals([
  "a0",
  "b0", "h4b20", "i2b20",                           // [[Has]]
  "c0", "j4c15", "k2c15",                           // [[Get]]
  "d0", "l4d11", "m2d11",                           // [[Set]]
  "e14", "r4e14", "q4r11e14", "s2e14",              // for-in [[Enumerate]]
      "o6e14", "q4e14", "p6e14", "q4e14", "q4e14",  // exhaust iterator
  "e9",                                             // for-in-body
      "h4e9","i2e9",                                // [[Has]] property
  "f2","foo", "e9",                                 // for-in-body
    "h4e9","i2e9",                                  // [[Has]]property
  "f2","bar", "e9",                                 // for-in-body
  "g0"
], log);
