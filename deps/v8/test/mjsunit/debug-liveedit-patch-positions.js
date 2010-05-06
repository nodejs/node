// Copyright 2010 the V8 project authors. All rights reserved.
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

// Scenario: some function is being edited; the outer function has to have its
// positions patched. Accoring to a special markup of function text
// corresponding byte-code PCs should conicide before change and after it.

Debug = debug.Debug

eval(
    "function F1() {  return 5; }\n" +
    "function ChooseAnimal(/*$*/ ) {\n" +
    "/*$*/ var x = F1(/*$*/ );\n" +
    "/*$*/ var res/*$*/  =/*$*/ (function() { return 'Cat'; } )();\n" +
    "/*$*/ var y/*$*/  = F2(/*$*/ F1()/*$*/ , F1(/*$*/ )/*$*/ );\n" +
    "/*$*/ if (/*$*/ x.toString(/*$*/ )) { /*$*/ y = 3;/*$*/  } else {/*$*/  y = 8;/*$*/  }\n" +
    "/*$*/ var z = /*$*/ x * y;\n" +
    "/*$*/ return/*$*/  res/*$*/  + z;/*$*/  }\n" +
    "function F2(x, y) { return x + y; }"
);

// Find all *$* markers in text of the function and read corresponding statement
// PCs.
function ReadMarkerPositions(func) {
  var text = func.toString();
  var positions = new Array();
  var match;
  var pattern = /\/\*\$\*\//g;
  while ((match = pattern.exec(text)) != null) {
    positions.push(match.index);
  }
  return positions;
}

function ReadPCMap(func, positions) {
  var res = new Array();
  for (var i = 0; i < positions.length; i++) {
    res.push(Debug.LiveEdit.GetPcFromSourcePos(func, positions[i]));
  }
  return res;
}

var res = ChooseAnimal();
assertEquals("Cat15", res);

var markerPositionsBefore = ReadMarkerPositions(ChooseAnimal);
var pcArrayBefore = ReadPCMap(ChooseAnimal, markerPositionsBefore);

var script = Debug.findScript(ChooseAnimal);

var orig_animal = "'Cat'";
var patch_pos = script.source.indexOf(orig_animal);
var new_animal_patch = "'Capybara'";

var change_log = new Array();
Debug.LiveEdit.TestApi.ApplySingleChunkPatch(script, patch_pos, orig_animal.length, new_animal_patch, change_log);
print("Change log: " + JSON.stringify(change_log) + "\n");

var res = ChooseAnimal();
assertEquals("Capybara15", res);

var markerPositionsAfter = ReadMarkerPositions(ChooseAnimal);
var pcArrayAfter = ReadPCMap(ChooseAnimal, markerPositionsAfter);

assertArrayEquals(pcArrayBefore, pcArrayAfter);

