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

// test that errors propagated from cluster children are properly received in their master
// creates an EADDRINUSE condition by also forking a child process to listen on a socket

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var fork = require('child_process').fork;
var fs = require('fs');
var net = require('net');


if (cluster.isMaster) {
  var worker = cluster.fork();
  var gotError = 0;
  worker.on('message', function(err) {
    gotError++;
    console.log(err);
    assert.strictEqual('EADDRINUSE', err.code);
    worker.disconnect();
  });
  process.on('exit', function() {
    console.log('master exited');
    try {
      fs.unlinkSync(common.PIPE);
    } catch (e) {
    }
    assert.equal(gotError, 1);
  });
} else {
  var cp = fork(common.fixturesDir + '/listen-on-socket-and-exit.js', { stdio: 'inherit' });

  // message from the child indicates it's ready and listening
  cp.on('message', function() {
    var server = net.createServer().listen(common.PIPE, function() {
      console.log('parent listening, should not be!');
    });

    server.on('error', function(err) {
      console.log('parent error, ending');
      // message to child process tells it to exit
      cp.send('end');
      // propagate error to parent
      process.send(err);
    });
  });
}
