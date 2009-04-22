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

// Set and remove a script break point for a named script.
var sbp = Debug.setScriptBreakPointByName("1", 2, 3);
assertEquals(1, Debug.scriptBreakPoints().length);
assertEquals("1", Debug.scriptBreakPoints()[0].script_name());
assertEquals(2, Debug.scriptBreakPoints()[0].line());
assertEquals(3, Debug.scriptBreakPoints()[0].column());
Debug.clearBreakPoint(sbp);
assertEquals(0, Debug.scriptBreakPoints().length);

// Set three script break points for named scripts.
var sbp1 = Debug.setScriptBreakPointByName("1", 2, 3);
var sbp2 = Debug.setScriptBreakPointByName("2", 3, 4);
var sbp3 = Debug.setScriptBreakPointByName("3", 4, 5);

// Check the content of the script break points.
assertEquals(3, Debug.scriptBreakPoints().length);
for (var i = 0; i < Debug.scriptBreakPoints().length; i++) {
  var x = Debug.scriptBreakPoints()[i];
  if ("1" == x.script_name()) {
    assertEquals(2, x.line());
    assertEquals(3, x.column());
  } else if ("2" == x.script_name()) {
    assertEquals(3, x.line());
    assertEquals(4, x.column());
  } else if ("3" == x.script_name()) {
    assertEquals(4, x.line());
    assertEquals(5, x.column());
  } else {
    assertUnreachable("unecpected script_name " + x.script_name());
  }
}

// Remove script break points (in another order than they where added).
assertEquals(3, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp1);
assertEquals(2, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp3);
assertEquals(1, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp2);
assertEquals(0, Debug.scriptBreakPoints().length);

// Set and remove a script break point for a script id.
var sbp = Debug.setScriptBreakPointById(1, 2, 3);
assertEquals(1, Debug.scriptBreakPoints().length);
assertEquals(1, Debug.scriptBreakPoints()[0].script_id());
assertEquals(2, Debug.scriptBreakPoints()[0].line());
assertEquals(3, Debug.scriptBreakPoints()[0].column());
Debug.clearBreakPoint(sbp);
assertEquals(0, Debug.scriptBreakPoints().length);

// Set three script break points for script ids.
var sbp1 = Debug.setScriptBreakPointById(1, 2, 3);
var sbp2 = Debug.setScriptBreakPointById(2, 3, 4);
var sbp3 = Debug.setScriptBreakPointById(3, 4, 5);

// Check the content of the script break points.
assertEquals(3, Debug.scriptBreakPoints().length);
for (var i = 0; i < Debug.scriptBreakPoints().length; i++) {
  var x = Debug.scriptBreakPoints()[i];
  if (1 == x.script_id()) {
    assertEquals(2, x.line());
    assertEquals(3, x.column());
  } else if (2 == x.script_id()) {
    assertEquals(3, x.line());
    assertEquals(4, x.column());
  } else if (3 == x.script_id()) {
    assertEquals(4, x.line());
    assertEquals(5, x.column());
  } else {
    assertUnreachable("unecpected script_id " + x.script_id());
  }
}

// Remove script break points (in another order than they where added).
assertEquals(3, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp1);
assertEquals(2, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp3);
assertEquals(1, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp2);
assertEquals(0, Debug.scriptBreakPoints().length);
