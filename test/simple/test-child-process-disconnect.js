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

var assert = require('assert');
var common = require('../common');
var fork = require('child_process').fork;
var net = require('net');

// child
if (process.argv[2] === 'child') {

  var server = net.createServer();

  server.on('connection', function(socket) {

    socket.resume();

    process.on('disconnect', function() {
      socket.end((process.connected).toString());
    });

    // when the socket is closed, we will close the server
    // allowing the process to self terminate
    socket.on('end', function() {
      server.close();
    });

    socket.write('ready');
  });

  // when the server is ready tell parent
  server.on('listening', function() {
    process.send('ready');
  });

  server.listen(common.PORT);

} else {
  // testcase
  var child = fork(process.argv[1], ['child']);

  var childFlag = false;
  var childSelfTerminate = false;
  var parentEmit = false;
  var parentFlag = false;

  // when calling .disconnect the event should emit
  // and the disconnected flag should be true.
  child.on('disconnect', function() {
    parentEmit = true;
    parentFlag = child.connected;
  });

  // the process should also self terminate without using signals
  child.on('exit', function() {
    childSelfTerminate = true;
  });

  // when child is listning
  child.on('message', function(msg) {
    if (msg === 'ready') {

      // connect to child using TCP to know if disconnect was emitted
      var socket = net.connect(common.PORT);

      socket.on('data', function(data) {
        data = data.toString();

        // ready to be disconnected
        if (data === 'ready') {
          child.disconnect();
          assert.throws(child.disconnect.bind(child), Error);
          return;
        }

        // disconnect is emitted
        childFlag = (data === 'true');
      });

    }
  });

  process.on('exit', function() {
    assert.equal(childFlag, false);
    assert.equal(parentFlag, false);

    assert.ok(childSelfTerminate);
    assert.ok(parentEmit);
  });
}
