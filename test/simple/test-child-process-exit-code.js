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
var spawn = require('child_process').spawn;
var path = require('path');

var exits = 0;

var exitScript = path.join(common.fixturesDir, 'exit.js');
var exitChild = spawn(process.argv[0], [exitScript, 23]);
exitChild.addListener('exit', function(code, signal) {
  assert.strictEqual(code, 23);
  assert.strictEqual(signal, null);

  exits++;
});



var errorScript = path.join(common.fixturesDir,
                            'child_process_should_emit_error.js');
var errorChild = spawn(process.argv[0], [errorScript]);
errorChild.addListener('exit', function(code, signal) {
  assert.ok(code !== 0);
  assert.strictEqual(signal, null);

  exits++;
});


process.addListener('exit', function() {
  assert.equal(2, exits);
});
