// Copyright 2009 the V8 project authors. All rights reserved.
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

// Load implementations from <project root>/tools.
// Files: tools/csvparser.js tools/splaytree.js tools/codemap.js
// Files: tools/consarray.js tools/profile.js tools/profile_view.js
// Files: tools/logreader.js tools/arguments.js tools/tickprocessor.js
// Files: tools/profviz/composer.js
// Env: TEST_FILE_NAME

assertEquals('string', typeof TEST_FILE_NAME);
var path_length = TEST_FILE_NAME.lastIndexOf('/');
if (path_length == -1) {
  path_length = TEST_FILE_NAME.lastIndexOf('\\');
}
assertTrue(path_length != -1);

var path = TEST_FILE_NAME.substr(0, path_length + 1);
var input_file = path + "profviz-test.log";
var reference_file = path + "profviz-test.default";

var content_lines = read(input_file).split("\n");
var line_cursor = 0;
var output_lines = [];

function input() {
  return content_lines[line_cursor++];
}

function output(line) {
  output_lines.push(line);
}

function set_range(start, end) {
  range_start = start;
  range_end = end;
}

var distortion = 4500 / 1000000;
var resx = 1600;
var resy = 600;

var psc = new PlotScriptComposer(resx, resy);
psc.collectData(input, distortion);
psc.findPlotRange(undefined, undefined, set_range);
var objects = psc.assembleOutput(output);

output("# start: " + range_start);
output("# end: " + range_end);
output("# objects: " + objects);

var create_baseline = false;

if (create_baseline) {
  print(JSON.stringify(output_lines, null, 2));
} else {
  assertArrayEquals(JSON.parse(read(reference_file)), output_lines);
}
