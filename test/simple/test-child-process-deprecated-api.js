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
var fs = require('fs');
var exits = 0;

// Test `env` parameter
// for child_process.spawn(path, args, env, customFds) deprecated api
(function() {
  var response = '';
  var child = spawn('/usr/bin/env', [], {'HELLO': 'WORLD'});

  child.stdout.setEncoding('utf8');

  child.stdout.addListener('data', function(chunk) {
    response += chunk;
  });

  process.addListener('exit', function() {
    assert.ok(response.indexOf('HELLO=WORLD') >= 0);
    exits++;
  });
})();

// Test `customFds` parameter
// for child_process.spawn(path, args, env, customFds) deprecated api
(function() {
  var expected = 'hello world';
  var helloPath = path.join(common.tmpDir, 'hello.txt');

  fs.open(helloPath, 'w', 400, function(err, fd) {
    if (err) throw err;

    var child = spawn('/bin/echo', [expected], undefined, [-1, fd]);

    assert.notEqual(child.stdin, null);
    assert.equal(child.stdout, null);
    assert.notEqual(child.stderr, null);

    child.addListener('exit', function(err) {
      if (err) throw err;

      fs.close(fd, function(err) {
        if (err) throw err;

        fs.readFile(helloPath, function(err, data) {
          if (err) throw err;

          assert.equal(data.toString(), expected + '\n');
          exits++;
        });
      });
    });
  });
})();

// Check if all child processes exited
process.addListener('exit', function() {
  assert.equal(2, exits);
});
