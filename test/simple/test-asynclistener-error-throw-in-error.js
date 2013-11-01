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

var checkStr = 'WRITTEN ON EXIT';

if (process.argv[2] === 'child')
  runChild();
else
  runParent();


function runChild() {
  var cntr = 0;

  var key = process.addAsyncListener(function() { }, {
    error: function onError() {
      cntr++;
      throw new Error('onError');
    }
  });

  process.on('unhandledException', function() {
    // Throwing in 'error' should bypass unhandledException.
    process.exit(2);
  });

  process.on('exit', function() {
    // Make sure that we can still write out to stderr even when the
    // process dies.
    process._rawDebug(checkStr);
  });

  process.nextTick(function() {
    throw new Error('nextTick');
  });
}


function runParent() {
  var childDidExit = false;
  var childStr = '';
  var child = spawn(process.execPath, [__filename, 'child']);
  child.stderr.on('data', function(chunk) {
    process._rawDebug('received data from child');
    childStr += chunk.toString();
  });

  child.on('exit', function(code) {
    process._rawDebug('child process exiting');
    childDidExit = true;
    // This is thrown when Node throws from _fatalException.
    assert.equal(code, 7);
  });

  process.on('exit', function() {
    process._rawDebug('child ondata message:',
                childStr.substr(0, checkStr.length));

    assert.ok(childDidExit);
    assert.equal(childStr.substr(0, checkStr.length), checkStr);
    console.log('ok');
  });
}

