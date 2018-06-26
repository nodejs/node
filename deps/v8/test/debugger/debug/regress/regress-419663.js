// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var o = {
  f: function(x) {
    var a = x + 1;
    o = 1;
  }
}

function sentinel() {}

var Debug = debug.Debug;

Debug.setListener(function() {});

var script = Debug.findScript(sentinel);

// Used in Debug.setScriptBreakPointById.
var p = Debug.findScriptSourcePosition(script, 8, 0);
var q = Debug.setBreakPointByScriptIdAndPosition(script.id, p).actual_position;

function assertLocation(p, l, c) {
  var location = %ScriptPositionInfo(script, p, false);
  assertEquals(l, location.line);
  assertEquals(c, location.column);
}

assertLocation(p, 8, 0);
assertLocation(q, 8, 4);
