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

if (process.argv[2] === 'child') {

  var endMe = null;

  process.on('message', function(m, socket) {
    if (!socket) return;

    // will call .end('end') or .write('write');
    socket[m](m);

    // store the unfinished socket
    if (m === 'write') {
      endMe = socket;
    }
  });

  process.on('message', function(m) {
    if (m !== 'close') return;
    endMe.end('end');
    endMe = null;
  });

  process.on('disconnect', function() {
    endMe.end('end');
    endMe = null;
  });

} else {

  var child1 = fork(process.argv[1], ['child']);
  var child2 = fork(process.argv[1], ['child']);
  var child3 = fork(process.argv[1], ['child']);

  var server = net.createServer();

  var connected = 0;
  server.on('connection', function(socket) {
    switch (connected) {
      case 0:
        child1.send('end', socket); break;
      case 1:
        child1.send('write', socket); break;
      case 2:
        child2.send('end', socket); break;
      case 3:
        child2.send('write', socket); break;
      case 4:
        child3.send('end', socket); break;
      case 5:
        child3.send('write', socket); break;
    }
    connected += 1;

    if (connected === 6) {
      closeServer();
    }
  });

  var disconnected = 0;
  server.on('listening', function() {

    var j = 6, client;
    while (j--) {
      client = net.connect(common.PORT, '127.0.0.1');
      client.on('close', function() {
        disconnected += 1;
      });
    }
  });

  var closeEmitted = false;
  server.on('close', function() {
    closeEmitted = true;

    child1.kill();
    child2.kill();
    child3.kill();
  });

  server.listen(common.PORT, '127.0.0.1');

  var timeElasped = 0;
  var closeServer = function() {
    var startTime = Date.now();
    server.on('close', function() {
      timeElasped = Date.now() - startTime;
    });

    server.close();

    setTimeout(function() {
      child1.send('close');
      child2.send('close');
      child3.disconnect();
    }, 200);
  };

  process.on('exit', function() {
    assert.equal(disconnected, 6);
    assert.equal(connected, 6);
    assert.ok(closeEmitted);
    assert.ok(timeElasped >= 190 && timeElasped <= 1000,
              'timeElasped was not between 190 and 1000 ms');
  });
}
