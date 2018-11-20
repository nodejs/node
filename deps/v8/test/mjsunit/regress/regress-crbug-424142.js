// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

(function outer() {
  var C = (function C_() {
    var y = 1;
    function CC() {
      this.x = 0;
    }
    CC.prototype.f = function CCf() {
      this.x += y;
      return this.x;
    };
    return CC;
  })();

  var c = new C(0);
})

function sentinel() {}

Debug = debug.Debug;

var script = Debug.findScript(sentinel);
var line = 14;
var line_start = Debug.findScriptSourcePosition(script, line, 0);
var line_end = Debug.findScriptSourcePosition(script, line + 1, 0) - 1;
var actual = Debug.setBreakPointByScriptIdAndPosition(
                 script.id, line_start).actual_position;
// Make sure the actual break position is within the line where we set
// the break point.
assertTrue(line_start <= actual);
assertTrue(actual <= line_end);
