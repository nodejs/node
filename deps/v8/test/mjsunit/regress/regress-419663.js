// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-debug-as debug

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
var p = Debug.findScriptSourcePosition(script, 9, 0);
var q = Debug.setBreakPointByScriptIdAndPosition(script.id, p).actual_position;
var r = Debug.setBreakPointByScriptIdAndPosition(script.id, q).actual_position;

assertEquals(q, r);

function assertLocation(p, l, c) {
  var location = script.locationFromPosition(p, false);
  assertEquals(l, location.line);
  assertEquals(c, location.column);
}

assertLocation(p, 9, 0);
assertLocation(q, 9, 4);
assertLocation(r, 9, 4);
