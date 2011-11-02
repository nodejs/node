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




var common = require('../common');
var assert = require('assert');
var path = require('path');
var childProccess = require('child_process');
var fs = require('fs');

var scriptString = path.join(common.fixturesDir, 'print-chars.js');
var scriptBuffer = path.join(common.fixturesDir, 'print-chars-from-buffer.js');
var tmpFile = path.join(common.fixturesDir, 'stdout.txt');

function test(size, useBuffer, cb) {
  var cmd = '"' + process.argv[0] + '"' +
            ' ' +
            '"' + (useBuffer ? scriptBuffer : scriptString) + '"' +
            ' ' +
            size +
            ' > ' +
            '"' + tmpFile + '"';

  try {
    fs.unlinkSync(tmpFile);
  } catch (e) {}

  common.print(size + ' chars to ' + tmpFile + '...');

  childProccess.exec(cmd, function(err) {
    if (err) throw err;

    console.log('done!');

    var stat = fs.statSync(tmpFile);

    console.log(tmpFile + ' has ' + stat.size + ' bytes');

    assert.equal(size, stat.size);
    fs.unlinkSync(tmpFile);

    cb();
  });
}

var finished = false;
test(1024 * 1024, false, function() {
  console.log('Done printing with string');
  test(1024 * 1024, true, function() {
    console.log('Done printing with buffer');
    finished = true;
  });
});

process.on('exit', function() {
  assert.ok(finished);
});
