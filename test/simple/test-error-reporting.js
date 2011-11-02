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
var exec = require('child_process').exec;
var path = require('path');

var exits = 0;

function errExec(script, callback) {
  var cmd = '"' + process.argv[0] + '" "' +
      path.join(common.fixturesDir, script) + '"';
  return exec(cmd, function(err, stdout, stderr) {
    // There was some error
    assert.ok(err);

    // More than one line of error output.
    assert.ok(stderr.split('\n').length > 2);

    // Assert the script is mentioned in error output.
    assert.ok(stderr.indexOf(script) >= 0);

    // Proxy the args for more tests.
    callback(err, stdout, stderr);

    // Count the tests
    exits++;

    console.log('.');
  });
}


// Simple throw error
errExec('throws_error.js', function(err, stdout, stderr) {
  assert.ok(/blah/.test(stderr));
});


// Trying to JSON.parse(undefined)
errExec('throws_error2.js', function(err, stdout, stderr) {
  assert.ok(/SyntaxError/.test(stderr));
});


// Trying to JSON.parse(undefined) in nextTick
errExec('throws_error3.js', function(err, stdout, stderr) {
  assert.ok(/SyntaxError/.test(stderr));
});


process.on('exit', function() {
  assert.equal(3, exits);
});
