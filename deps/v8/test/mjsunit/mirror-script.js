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

// Flags: --expose-debug-as debug --allow-natives-syntax
// Test the mirror object for scripts.

function testScriptMirror(f, file_name, file_lines, type, compilation_type,
                          source, eval_from_line) {
  // Create mirror and JSON representation.
  var mirror = debug.MakeMirror(f).script();
  var serializer = debug.MakeMirrorSerializer();
  var json = JSON.stringify(serializer.serializeValue(mirror));

  // Check the mirror hierachy.
  assertTrue(mirror instanceof debug.Mirror);
  assertFalse(mirror instanceof debug.ValueMirror);
  assertTrue(mirror instanceof debug.ScriptMirror);

  // Check the mirror properties.
  assertTrue(mirror.isScript());
  assertEquals('script', mirror.type());
  var name = mirror.name();
  if (name) {
    assertEquals(file_name, name.substring(name.length - file_name.length));
  } else {
    assertTrue(file_name === null);
  }
  assertEquals(0, mirror.lineOffset());
  assertEquals(0, mirror.columnOffset());
  if (file_lines > 0) {
    assertEquals(file_lines, mirror.lineCount());
  }
  assertEquals(type, mirror.scriptType());
  assertEquals(compilation_type, mirror.compilationType(), "compilation type");
  if (source) {
    assertEquals(source, mirror.source());
  }
  if (eval_from_line) {
    assertEquals(eval_from_line,  mirror.evalFromLocation().line);
  }

  // Parse JSON representation and check.
  var fromJSON = JSON.parse(json);
  assertEquals('script', fromJSON.type);
  name = fromJSON.name;
  if (name) {
    assertEquals(file_name, name.substring(name.length - file_name.length));
  } else {
    assertTrue(file_name === null);
  }
  assertEquals(0, fromJSON.lineOffset);
  assertEquals(0, fromJSON.columnOffset);
  if (file_lines > 0) {
    assertEquals(file_lines, fromJSON.lineCount);
  }
  assertEquals(type, fromJSON.scriptType);
  assertEquals(compilation_type, fromJSON.compilationType);
}


// Test the script mirror for different functions.
testScriptMirror(function(){}, 'mirror-script.js', 90, 2, 0);
testScriptMirror(Math.random, 'native math.js', -1, 0, 0);
testScriptMirror(eval('(function(){})'), null, 1, 2, 1, '(function(){})', 87);
testScriptMirror(eval('(function(){\n  })'), null, 2, 2, 1, '(function(){\n  })', 88);
