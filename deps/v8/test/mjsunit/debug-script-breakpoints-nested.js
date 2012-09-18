// Copyright 2012 the V8 project authors. All rights reserved.
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

// Simple debug event handler which just counts the number of break points hit.
var break_point_hit_count = 0;

function listener(event, exec_state, event_data, data) {
  if (event == Debug.DebugEvent.Break) {
    break_point_hit_count++;
  }
};

// Add the debug event listener.
Debug.setListener(listener);

eval(
  "var inner;\n" +
  "function outer() {\n" +         // Non-trivial outer closure.
  "  var x = 5;\n" +
  "  function a() {\n" +
  "    var foo = 0, y = 7;\n" +
  "    function b() {\n" +
  "      var bar = 0, baz = 0, z = 11;\n" +
  "      function c() {\n" +
  "        return x + y + z;\n" +  // Breakpoint line ( #8 )
  "      }\n" +
  "      inner = c;\n" +
  "      return c();\n" +
  "    }\n" +
  "    return b();\n" +
  "  }\n" +
  "  return a();\n" +
  "}"
);

var script = Debug.findScript(outer);

// The debugger triggers compilation of inner closures.
assertEquals(0, Debug.scriptBreakPoints().length);
var sbp = Debug.setScriptBreakPointById(script.id, 8);
assertEquals(1, Debug.scriptBreakPoints().length);

// The compiled outer closure should behave correctly.
assertEquals(23, outer());
assertEquals(1, break_point_hit_count);

// The compiled inner closure should behave correctly.
assertEquals(23, inner());
assertEquals(2, break_point_hit_count);

// Remove script break point.
assertEquals(1, Debug.scriptBreakPoints().length);
Debug.clearBreakPoint(sbp);
assertEquals(0, Debug.scriptBreakPoints().length);
