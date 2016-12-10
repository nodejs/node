// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug
Debug.setListener(function(){});

var script_id;
var script_name;

// Get current script id and name.
var scripts = Debug.scripts();
for (var i = 0; i < scripts.length; i++) {
  var name = scripts[i].name;
  var id = scripts[i].id;
  if (name !== undefined && name.includes("debug-script-breakpoints.js")) {
    script_id = id;
    script_name = name;
    break;
  }
}
assertTrue(script_id !== undefined);
assertTrue(script_name !== undefined);
print("#" + script_id + ": " + script_name);


// Checks script name, line and column.
var checkBreakPoint = function(id, line, column) {
  var breakpoint = Debug.scriptBreakPoints()[id];
  assertEquals(script_name, breakpoint.script_name());
  assertEquals(line, breakpoint.line());
  assertEquals(column, breakpoint.column());
}


// Set and remove a script break point for a named script.
var sbp = Debug.setScriptBreakPointByName(script_name, 35, 5);
assertEquals(1, Debug.scriptBreakPoints().length);
checkBreakPoint(0, 35, 5);
Debug.clearBreakPoint(sbp);
assertEquals(0, Debug.scriptBreakPoints().length);

// Set three script break points for named scripts.
var sbp1 = Debug.setScriptBreakPointByName(script_name, 36, 3);
var sbp2 = Debug.setScriptBreakPointByName(script_name, 37, 4);
var sbp3 = Debug.setScriptBreakPointByName(script_name, 38, 5);

// Check the content of the script break points.
assertEquals(3, Debug.scriptBreakPoints().length);
checkBreakPoint(0, 36, 3);
checkBreakPoint(1, 37, 4);
checkBreakPoint(2, 38, 5);

// Remove script break points (in another order than they where added).
assertEquals(3, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp1);
assertEquals(2, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp3);
assertEquals(1, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp2);
assertEquals(0, Debug.scriptBreakPoints().length);


// Checks script id, line and column.
var checkBreakPoint = function(id, line, column) {
  var breakpoint = Debug.scriptBreakPoints()[id];
  assertEquals(script_id, breakpoint.script_id());
  assertEquals(line, breakpoint.line());
  assertEquals(column, breakpoint.column());
}


// Set and remove a script break point for a script id.
var sbp = Debug.setScriptBreakPointById(script_id, 40, 6);
assertEquals(1, Debug.scriptBreakPoints().length);
checkBreakPoint(0, 40, 6);
Debug.clearBreakPoint(sbp);
assertEquals(0, Debug.scriptBreakPoints().length);

// Set three script break points for script ids.
var sbp1 = Debug.setScriptBreakPointById(script_id, 42, 3);
var sbp2 = Debug.setScriptBreakPointById(script_id, 43, 4);
var sbp3 = Debug.setScriptBreakPointById(script_id, 44, 5);

// Check the content of the script break points.
assertEquals(3, Debug.scriptBreakPoints().length);
checkBreakPoint(0, 42, 3);
checkBreakPoint(1, 43, 4);
checkBreakPoint(2, 44, 5);

// Remove script break points (in another order than they where added).
assertEquals(3, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp1);
assertEquals(2, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp3);
assertEquals(1, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp2);
assertEquals(0, Debug.scriptBreakPoints().length);

Debug.setListener(null);
