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
// For this test to work this file MUST have CR LF line endings.
function a() { b(); };
function    b() {
  c(true);
};
  function c(x) {
    if (x) {
      return 1;
    } else {
      return 1;
    }
  };
function d(x) {
  x = 1 ;
  x = 2 ;
  x = 3 ;
  x = 4 ;
  x = 5 ;
  x = 6 ;
  x = 7 ;
  x = 8 ;
  x = 9 ;
  x = 10;
  x = 11;
  x = 12;
  x = 13;
  x = 14;
  x = 15;
}

// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// This is the number of comment lines above the first test function.
var comment_lines = 29;

// This is the last position in the entire file (note: this equals
// file size of <debug-sourceinfo.js> - 1, since starting at 0).
var last_position = 14312;
// This is the last line of entire file (note: starting at 0).
var last_line = 351;
// This is the last column of last line (note: starting at 0 and +2, due
// to trailing <CR><LF>).
var last_column = 2;

// This magic number is the length or the first line comment (actually number
// of characters before 'function a(...'.
var comment_line_length = 1726;
var start_a = 10 + comment_line_length;
var start_b = 37 + comment_line_length;
var start_c = 71 + comment_line_length;
var start_d = 163 + comment_line_length;

// The position of the first line of d(), i.e. "x = 1 ;".
var start_code_d = start_d + 7;
// The line # of the first line of d() (note: starting at 0).
var start_line_d = 41;
var line_length_d = 11;
var num_lines_d = 15;

assertEquals(start_a, Debug.sourcePosition(a));
assertEquals(start_b, Debug.sourcePosition(b));
assertEquals(start_c, Debug.sourcePosition(c));
assertEquals(start_d, Debug.sourcePosition(d));

var script = Debug.findScript(a);
assertTrue(script.data === Debug.findScript(b).data);
assertTrue(script.data === Debug.findScript(c).data);
assertTrue(script.data === Debug.findScript(d).data);
assertTrue(script.source === Debug.findScript(b).source);
assertTrue(script.source === Debug.findScript(c).source);
assertTrue(script.source === Debug.findScript(d).source);

// Test that when running through source positions the position, line and
// column progresses as expected.
var position;
var line;
var column;
for (var p = 0; p < 100; p++) {
  var location = script.locationFromPosition(p);
  if (p > 0) {
    assertEquals(position + 1, location.position);
    if (line == location.line) {
      assertEquals(column + 1, location.column);
    } else {
      assertEquals(line + 1, location.line);
      assertEquals(0, location.column);
    }
  } else {
    assertEquals(0, location.position);
    assertEquals(0, location.line);
    assertEquals(0, location.column);
  }

  // Remember the location.
  position = location.position;
  line = location.line;
  column = location.column;
}

// Every line of d() is the same length.  Verify we can loop through all
// positions and find the right line # for each.
var p = start_code_d;
for (line = 0; line < num_lines_d; line++) {
  for (column = 0; column < line_length_d; column++) {
    var location = script.locationFromPosition(p);
    assertEquals(p, location.position);
    assertEquals(start_line_d + line, location.line);
    assertEquals(column, location.column);
    p++;
  }
}

// Test first position.
assertEquals(0, script.locationFromPosition(0).position);
assertEquals(0, script.locationFromPosition(0).line);
assertEquals(0, script.locationFromPosition(0).column);

// Test second position.
assertEquals(1, script.locationFromPosition(1).position);
assertEquals(0, script.locationFromPosition(1).line);
assertEquals(1, script.locationFromPosition(1).column);

// Test first position in function a().
assertEquals(start_a, script.locationFromPosition(start_a).position);
assertEquals(0, script.locationFromPosition(start_a).line - comment_lines);
assertEquals(10, script.locationFromPosition(start_a).column);

// Test first position in function b().
assertEquals(start_b, script.locationFromPosition(start_b).position);
assertEquals(1, script.locationFromPosition(start_b).line - comment_lines);
assertEquals(13, script.locationFromPosition(start_b).column);

// Test first position in function c().
assertEquals(start_c, script.locationFromPosition(start_c).position);
assertEquals(4, script.locationFromPosition(start_c).line - comment_lines);
assertEquals(12, script.locationFromPosition(start_c).column);

// Test first position in function d().
assertEquals(start_d, script.locationFromPosition(start_d).position);
assertEquals(11, script.locationFromPosition(start_d).line - comment_lines);
assertEquals(10, script.locationFromPosition(start_d).column);

// Test first line.
assertEquals(0, script.locationFromLine().position);
assertEquals(0, script.locationFromLine().line);
assertEquals(0, script.locationFromLine().column);
assertEquals(0, script.locationFromLine(0).position);
assertEquals(0, script.locationFromLine(0).line);
assertEquals(0, script.locationFromLine(0).column);

// Test first line column 1.
assertEquals(1, script.locationFromLine(0, 1).position);
assertEquals(0, script.locationFromLine(0, 1).line);
assertEquals(1, script.locationFromLine(0, 1).column);

// Test first line offset 1.
assertEquals(1, script.locationFromLine(0, 0, 1).position);
assertEquals(0, script.locationFromLine(0, 0, 1).line);
assertEquals(1, script.locationFromLine(0, 0, 1).column);

// Test offset function a().
assertEquals(start_a, script.locationFromLine(void 0, void 0, start_a).position);
assertEquals(0, script.locationFromLine(void 0, void 0, start_a).line - comment_lines);
assertEquals(10, script.locationFromLine(void 0, void 0, start_a).column);
assertEquals(start_a, script.locationFromLine(0, void 0, start_a).position);
assertEquals(0, script.locationFromLine(0, void 0, start_a).line - comment_lines);
assertEquals(10, script.locationFromLine(0, void 0, start_a).column);
assertEquals(start_a, script.locationFromLine(0, 0, start_a).position);
assertEquals(0, script.locationFromLine(0, 0, start_a).line - comment_lines);
assertEquals(10, script.locationFromLine(0, 0, start_a).column);

// Test second line offset function a().
assertEquals(start_a + 14, script.locationFromLine(1, 0, start_a).position);
assertEquals(1, script.locationFromLine(1, 0, start_a).line - comment_lines);
assertEquals(0, script.locationFromLine(1, 0, start_a).column);

// Test second line column 2 offset function a().
assertEquals(start_a + 14 + 2, script.locationFromLine(1, 2, start_a).position);
assertEquals(1, script.locationFromLine(1, 2, start_a).line - comment_lines);
assertEquals(2, script.locationFromLine(1, 2, start_a).column);

// Test offset function b().
assertEquals(start_b, script.locationFromLine(0, 0, start_b).position);
assertEquals(1, script.locationFromLine(0, 0, start_b).line - comment_lines);
assertEquals(13, script.locationFromLine(0, 0, start_b).column);

// Test second line offset function b().
assertEquals(start_b + 6, script.locationFromLine(1, 0, start_b).position);
assertEquals(2, script.locationFromLine(1, 0, start_b).line - comment_lines);
assertEquals(0, script.locationFromLine(1, 0, start_b).column);

// Test second line column 11 offset function b().
assertEquals(start_b + 6 + 11, script.locationFromLine(1, 11, start_b).position);
assertEquals(2, script.locationFromLine(1, 11, start_b).line - comment_lines);
assertEquals(11, script.locationFromLine(1, 11, start_b).column);

// Test second line column 12 offset function b. Second line in b is 11 long
// using column 12 wraps to next line.
assertEquals(start_b + 6 + 12, script.locationFromLine(1, 12, start_b).position);
assertEquals(3, script.locationFromLine(1, 12, start_b).line - comment_lines);
assertEquals(0, script.locationFromLine(1, 12, start_b).column);

// Test the Debug.findSourcePosition which wraps SourceManager.
assertEquals(0 + start_a, Debug.findFunctionSourceLocation(a, 0, 0).position);
assertEquals(0 + start_b, Debug.findFunctionSourceLocation(b, 0, 0).position);
assertEquals(6 + start_b, Debug.findFunctionSourceLocation(b, 1, 0).position);
assertEquals(8 + start_b, Debug.findFunctionSourceLocation(b, 1, 2).position);
assertEquals(18 + start_b, Debug.findFunctionSourceLocation(b, 2, 0).position);
assertEquals(0 + start_c, Debug.findFunctionSourceLocation(c, 0, 0).position);
assertEquals(7 + start_c, Debug.findFunctionSourceLocation(c, 1, 0).position);
assertEquals(21 + start_c, Debug.findFunctionSourceLocation(c, 2, 0).position);
assertEquals(38 + start_c, Debug.findFunctionSourceLocation(c, 3, 0).position);
assertEquals(52 + start_c, Debug.findFunctionSourceLocation(c, 4, 0).position);
assertEquals(69 + start_c, Debug.findFunctionSourceLocation(c, 5, 0).position);
assertEquals(76 + start_c, Debug.findFunctionSourceLocation(c, 6, 0).position);
assertEquals(0 + start_d, Debug.findFunctionSourceLocation(d, 0, 0).position);
assertEquals(7 + start_d, Debug.findFunctionSourceLocation(d, 1, 0).position);
for (i = 1; i <= num_lines_d; i++) {
  assertEquals(7 + (i * line_length_d) + start_d, Debug.findFunctionSourceLocation(d, (i + 1), 0).position);
}
assertEquals(175 + start_d, Debug.findFunctionSourceLocation(d, 17, 0).position);

// Make sure invalid inputs work properly.
assertEquals(0, script.locationFromPosition(-1).line);
assertEquals(null, script.locationFromPosition(last_position + 1));

// Test last position.
assertEquals(last_position, script.locationFromPosition(last_position).position);
assertEquals(last_line, script.locationFromPosition(last_position).line);
assertEquals(last_column, script.locationFromPosition(last_position).column);

// Test source line and restriction. All the following tests start from line 1
// column 2 in function b, which is the call to c.
//   c(true);
//   ^

var location;

location = script.locationFromLine(1, 0, start_b);
assertEquals('  c(true);', location.sourceText());

result = ['c', ' c', ' c(', '  c(', '  c(t']
for (var i = 1; i <= 5; i++) {
  location = script.locationFromLine(1, 2, start_b);
  location.restrict(i);
  assertEquals(result[i - 1], location.sourceText());
}

location = script.locationFromLine(1, 2, start_b);
location.restrict(1, 0);
assertEquals('c', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(2, 0);
assertEquals('c(', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(2, 1);
assertEquals(' c', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(2, 2);
assertEquals(' c', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(2, 3);
assertEquals(' c', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(3, 1);
assertEquals(' c(', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(5, 0);
assertEquals('c(tru', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(5, 2);
assertEquals('  c(t', location.sourceText());

location = script.locationFromLine(1, 2, start_b);
location.restrict(5, 4);
assertEquals('  c(t', location.sourceText());

// All the following tests start from line 1 column 10 in function b, which is
// the final character.
//   c(true);
//          ^

location = script.locationFromLine(1, 10, start_b);
location.restrict(5, 0);
assertEquals('rue);', location.sourceText());

location = script.locationFromLine(1, 10, start_b);
location.restrict(7, 0);
assertEquals('(true);', location.sourceText());

// All the following tests start from line 1 column 0 in function b, which is
// the first character.
//   c(true);
//^

location = script.locationFromLine(1, 0, start_b);
location.restrict(5, 0);
assertEquals('  c(t', location.sourceText());

location = script.locationFromLine(1, 0, start_b);
location.restrict(5, 4);
assertEquals('  c(t', location.sourceText());

location = script.locationFromLine(1, 0, start_b);
location.restrict(7, 0);
assertEquals('  c(tru', location.sourceText());

location = script.locationFromLine(1, 0, start_b);
location.restrict(7, 6);
assertEquals('  c(tru', location.sourceText());

// Test that script.sourceLine(line) works.
for (line = 0; line < num_lines_d; line++) {
  var line_content_regexp = new RegExp("  x = " + (line + 1));
  assertTrue(line_content_regexp.test(script.sourceLine(start_line_d + line)));
}
