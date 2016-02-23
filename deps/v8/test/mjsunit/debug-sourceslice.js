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
// Source lines for test.
var lines = [ 'function a() { b(); };\n',
              'function    b() {\n',
              '  c(true);\n',
              '};\n',
              '  function c(x) {\n',
              '    if (x) {\n',
              '      return 1;\n',
              '    } else {\n',
              '      return 1;\n',
              '    }\n',
              '  };\n' ];

// Build source by putting all lines together
var source = '';
for (var i = 0; i < lines.length; i++) {
  source += lines[i];
}
eval(source);

// Flags: --expose-debug-as debug
// Get the Debug object exposed from the debug context global object.
Debug = debug.Debug

// Get the script object from one of the functions in the source.
var script = Debug.findScript(a);

// Make sure that the source is as expected.
assertEquals(source, script.source);
assertEquals(source, script.sourceSlice().sourceText());

// Try all possible line interval slices.
for (var slice_size = 0; slice_size < lines.length; slice_size++) {
  for (var n = 0; n < lines.length - slice_size; n++) {
    var slice = script.sourceSlice(n, n + slice_size);
    assertEquals(n, slice.from_line);
    assertEquals(n + slice_size, slice.to_line);

    var text = slice.sourceText();
    var expected = '';
    for (var i = 0; i < slice_size; i++) {
      expected += lines[n + i];
    }
    assertEquals(expected, text);
  }
}
