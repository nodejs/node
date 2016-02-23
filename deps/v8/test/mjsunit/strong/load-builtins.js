// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

function getGlobal() {
  return this;
}

function polluteGlobal() {
  bar = 0;
}

(function() {
  "use strict";

  let builtins = [
    Array,
    Object,
    Function,
    getGlobal()
  ];

  for (let builtin of builtins) {
    assertThrows(function(){"use strong"; builtin.foo}, TypeError);
    assertThrows(function(){"use strong"; builtin[0]}, TypeError);
    assertThrows(function(){"use strong"; builtin[10000]}, TypeError);
    builtin.foo = 1;
    assertDoesNotThrow(function(){"use strong"; builtin.foo});
    assertThrows(function(){"use strong"; builtin.bar});
    assertThrows(function(){"use strong"; builtin[0]}, TypeError);
    assertThrows(function(){"use strong"; builtin[10000]}, TypeError);
    builtin[0] = 1;
    assertDoesNotThrow(function(){"use strong"; builtin.foo});
    assertThrows(function(){"use strong"; builtin.bar});
    assertDoesNotThrow(function(){"use strong"; builtin[0]});
    assertThrows(function(){"use strong"; builtin[10000]}, TypeError);
  }
  polluteGlobal();
  assertDoesNotThrow(function(){"use strong"; getGlobal().bar});
})();
