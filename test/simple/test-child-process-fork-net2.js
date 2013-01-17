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
var count = 12;

if (process.argv[2] === 'child') {

  var needEnd = [];

  process.on('message', function(m, socket) {
    if (!socket) return;

    console.error('got socket', m);

    // will call .end('end') or .write('write');
    socket[m](m);

    socket.resume();

    socket.on('data', function() {
      console.error('%d socket.data', process.pid, m);
    });

    socket.on('end', function() {
      console.error('%d socket.end', process.pid, m);
    });

    // store the unfinished socket
    if (m === 'write') {
      needEnd.push(socket);
    }

    socket.on('close', function() {
      console.error('%d socket.close', process.pid, m);
    });

    socket.on('finish', function() {
      console.error('%d socket finished', process.pid, m);
    });
  });

  process.on('message', function(m) {
    if (m !== 'close') return;
    console.error('got close message');
    needEnd.forEach(function(endMe, i) {
      console.error('%d ending %d', process.pid, i);
      endMe.end('end');
    });
  });

  process.on('disconnect', function() {
    console.error('%d process disconnect, ending', process.pid);
    needEnd.forEach(function(endMe, i) {
      console.error('%d ending %d', process.pid, i);
      endMe.end('end');
    });
    endMe = null;
  });

} else {

  var child1 = fork(process.argv[1], ['child']);
  var child2 = fork(process.argv[1], ['child']);
  var child3 = fork(process.argv[1], ['child']);

  var server = net.createServer();

  var connected = 0;
  server.on('connection', function(socket) {
    switch (connected % 6) {
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

    if (connected === count) {
      closeServer();
    }
  });

  var disconnected = 0;
  server.on('listening', function() {

    var j = count, client;
    while (j--) {
      client = net.connect(common.PORT, '127.0.0.1');
      client.on('close', function() {
        console.error('CLIENT: close event in master');
        disconnected += 1;
      });
      // XXX This resume() should be unnecessary.
      // a stream high water mark should be enough to keep
      // consuming the input.
      client.resume();
    }
  });

  var closeEmitted = false;
  server.on('close', function() {
    console.error('server close');
    closeEmitted = true;

    child1.kill();
    child2.kill();
    child3.kill();
  });

  server.listen(common.PORT, '127.0.0.1');

  var timeElasped = 0;
  var closeServer = function() {
    console.error('closeServer');
    var startTime = Date.now();
    server.on('close', function() {
      console.error('emit(close)');
      timeElasped = Date.now() - startTime;
    });

    console.error('calling server.close');
    server.close();

    setTimeout(function() {
      console.error('sending close to children');
      child1.send('close');
      child2.send('close');
      child3.disconnect();
    }, 200);
  };

  process.on('exit', function() {
    assert.equal(disconnected, count);
    assert.equal(connected, count);
    assert.ok(closeEmitted);
    assert.ok(timeElasped >= 190 && timeElasped <= 1000,
              'timeElasped was not between 190 and 1000 ms');
  });
}
