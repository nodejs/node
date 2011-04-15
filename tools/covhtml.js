// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// This tool is to make pretty HTML from --cov output.

var fs = require('fs');
var path = require('path');

var jsonFilename = process.argv[2];
if (!jsonFilename) {
  console.error("covhtml.js node-cov.json > out.html");
  process.exit(1);
}

var jsonFile = fs.readFileSync(jsonFilename);
var cov = JSON.parse(jsonFile);

var out = '<html><style>pre { margin: 0; padding: 0; } </style><body>';

for (var fn in cov) {
  var source = fs.readFileSync(fn, 'utf8');
  var lines = source.split('\n');

  out += '<h2>' + path.basename(fn) + '</h2>\n<div>\n';

  for (var i = 0; i < lines.length; i++) {
    lines[i] = lines[i].replace('<', '&lt;');
    lines[i] = lines[i].replace('>', '&gt;');
    if (cov[fn][i]) {
      out += '<pre>'
    } else {
      out += '<pre style="background: #faa">'
    }
    out += lines[i] + '</pre>\n';
  }

  out += '</div>\n';
}

out += '</body></html>'

console.log(out);
