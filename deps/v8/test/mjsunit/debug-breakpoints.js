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

function f() {a=1;b=2}
function g() {
  a=1;
  b=2;
}

bp = Debug.setBreakPoint(f, 0, 0);
assertEquals("() {[B0]a=1;b=2}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp);
assertEquals("() {a=1;b=2}", Debug.showBreakPoints(f));
bp1 = Debug.setBreakPoint(f, 0, 8);
assertEquals("() {a=1;[B0]b=2}", Debug.showBreakPoints(f));
bp2 = Debug.setBreakPoint(f, 0, 4);
assertEquals("() {[B0]a=1;[B1]b=2}", Debug.showBreakPoints(f));
bp3 = Debug.setBreakPoint(f, 0, 11);
assertEquals("() {[B0]a=1;[B1]b=2[B2]}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp1);
assertEquals("() {[B0]a=1;b=2[B1]}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp2);
assertEquals("() {a=1;b=2[B0]}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp3);
assertEquals("() {a=1;b=2}", Debug.showBreakPoints(f));

// The following test checks that the Debug.showBreakPoints(g) produces output
// like follows when changein breakpoints.
//
// function g() {
//   [BX]a=1;
//   [BX]b=2;
// }[BX]

// Test set and clear breakpoint at the first possible location (line 0,
// position 0).
bp = Debug.setBreakPoint(g, 0, 0);
// function g() {
//   [B0]a=1;
//   b=2;
// }
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
Debug.clearBreakPoint(bp);
// function g() {
//   a=1;
//   b=2;
// }
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]") < 0);

// Second test set and clear breakpoints on lines 1, 2 and 3 (position = 0).
bp1 = Debug.setBreakPoint(g, 2, 0);
// function g() {
//   a=1;
//   [B0]b=2;
// }
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]b=2;") > 0);
bp2 = Debug.setBreakPoint(g, 1, 0);
// function g() {
//   [B0]a=1;
//   [B1]b=2;
// }
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]b=2;") > 0);
bp3 = Debug.setBreakPoint(g, 3, 0);
// function g() {
//   [B0]a=1;
//   [B1]b=2;
// }[B2]
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]b=2;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B2]}") > 0);
Debug.clearBreakPoint(bp1);
// function g() {
//   [B0]a=1;
//   b=2;
// }[B1]
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]}") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B2]") < 0);
Debug.clearBreakPoint(bp2);
// function g() {
//   a=1;
//   b=2;
// }[B0]
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]}") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]") < 0);
Debug.clearBreakPoint(bp3);
// function g() {
//   a=1;
//   b=2;
// }
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]") < 0);


// Tests for setting break points by script id and position.
function setBreakpointByPosition(f, position, opt_position_alignment)
{
  var break_point = Debug.setBreakPointByScriptIdAndPosition(
      Debug.findScript(f).id,
      position + Debug.sourcePosition(f),
      "",
      true, opt_position_alignment);
  return break_point.number();
}

bp = setBreakpointByPosition(f, 0);
assertEquals("() {[B0]a=1;b=2}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp);
assertEquals("() {a=1;b=2}", Debug.showBreakPoints(f));
bp1 = setBreakpointByPosition(f, 8);
assertEquals("() {a=1;[B0]b=2}", Debug.showBreakPoints(f));
bp2 = setBreakpointByPosition(f, 4);
assertEquals("() {[B0]a=1;[B1]b=2}", Debug.showBreakPoints(f));
bp3 = setBreakpointByPosition(f, 11);
assertEquals("() {[B0]a=1;[B1]b=2[B2]}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp1);
assertEquals("() {[B0]a=1;b=2[B1]}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp2);
assertEquals("() {a=1;b=2[B0]}", Debug.showBreakPoints(f));
Debug.clearBreakPoint(bp3);
assertEquals("() {a=1;b=2}", Debug.showBreakPoints(f));

bp = setBreakpointByPosition(g, 0);
//function g() {
//[B0]a=1;
//b=2;
//}
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
Debug.clearBreakPoint(bp);
//function g() {
//a=1;
//b=2;
//}
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]") < 0);

//Second test set and clear breakpoints on lines 1, 2 and 3 (column = 0).
bp1 = setBreakpointByPosition(g, 12);
//function g() {
//a=1;
//[B0]b=2;
//}
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]b=2;") > 0);
bp2 = setBreakpointByPosition(g, 5);
//function g() {
//[B0]a=1;
//[B1]b=2;
//}
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]b=2;") > 0);
bp3 = setBreakpointByPosition(g, 19);
//function g() {
//[B0]a=1;
//[B1]b=2;
//}[B2]
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]b=2;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B2]}") > 0);
Debug.clearBreakPoint(bp1);
//function g() {
//[B0]a=1;
//b=2;
//}[B1]
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]a=1;") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]}") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B2]") < 0);
Debug.clearBreakPoint(bp2);
//function g() {
//a=1;
//b=2;
//}[B0]
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]}") > 0);
assertTrue(Debug.showBreakPoints(g).indexOf("[B1]") < 0);
Debug.clearBreakPoint(bp3);
//function g() {
//a=1;
//b=2;
//}
assertTrue(Debug.showBreakPoints(g).indexOf("[B0]") < 0);

// Tests for setting break points without statement aligment.
// (This may be sensitive to compiler break position map generation).
function h() {a=f(f2(1,2),f3())+f3();b=f3();}
var scenario = [
  [5, "{a[B0]=f"],
  [6, "{a=[B0]f("],
  [7, "{a=f([B0]f2("],
  [16, "f2(1,2),[B0]f3()"],
  [22, "+[B0]f3()"]
];
for(var i = 0; i < scenario.length; i++) {
  bp1 = setBreakpointByPosition(h, scenario[i][0],
      Debug.BreakPositionAlignment.BreakPosition);
  assertTrue(Debug.showBreakPoints(h, undefined,
      Debug.BreakPositionAlignment.BreakPosition).indexOf(scenario[i][1]) > 0);
  Debug.clearBreakPoint(bp1);
}
