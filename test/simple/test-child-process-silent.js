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
var fork = require('child_process').fork;

var isChild = process.argv[2] === 'child';

if (isChild) {
  console.log('LOG: a stdout message');
  console.error('LOG: a stderr message');

  process.send('message from child');
  process.once('message', function () {
    process.send('got message from master');
  });

} else {

  var checks = {
    stdoutNotPiped: false,
    stderrNotPiped: false,
    childSending: false,
    childReciveing: false
  };

  var child = fork(process.argv[1], ['child'], { silent: true });
  child.on('message', function (message) {
    if (checks.childSending === false) {
      checks.childSending = (message === 'message from child');
    }

    if (checks.childReciveing === false) {
      checks.childReciveing = (message === 'got message from master');
    }

    if (checks.childReciveing === true) {
      child.kill();
    }
  });

  checks.stdoutNotPiped = (child.stdout && child.stdout.readable === true);
  checks.stderrNotPiped = (child.stderr && child.stderr.readable === true);

  child.send('message to child');

  //Check all values
  process.once('exit', function() {

    //Check message system
    assert.ok(checks.childSending, 'The child was able to send a message');
    assert.ok(checks.childReciveing, 'The child was able to receive a message');

    //Check std(out|err) pipes
    assert.ok(checks.stdoutNotPiped, 'The stdout socket was piped to parent');
    assert.ok(checks.stdoutNotPiped, 'The stderr socket was piped to parent');
  });
}
