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

assert = require('assert');
child = require('child_process');

nodejs = '"' + process.execPath + '"';

if (module.parent) {
  // signal we've been loaded as a module
  console.log('Loaded as a module, exiting with status code 42.');
  process.exit(42);
}

// assert that the result of the final expression is written to stdout
child.exec(nodejs + ' --eval "1337; 42"',
    function(err, stdout, stderr) {
      assert.equal(parseInt(stdout), 42);
    });

// assert that module loading works
child.exec(nodejs + ' --eval "require(\'' + __filename + '\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// module path resolve bug, regression test
child.exec(nodejs + ' --eval "require(\'./test/simple/test-cli-eval.js\')"',
    function(status, stdout, stderr) {
      assert.equal(status.code, 42);
    });

// empty program should do nothing
child.exec(nodejs + ' -e ""', function(status, stdout, stderr) {
  assert.equal(stdout, 'undefined\n');
  assert.equal(stderr, '');
});
