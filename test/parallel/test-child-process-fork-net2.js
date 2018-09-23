'use strict';
var assert = require('assert');
var common = require('../common');
var fork = require('child_process').fork;
var net = require('net');
var count = 12;

if (process.argv[2] === 'child') {
  var needEnd = [];
  var id = process.argv[3];

  process.on('message', function(m, socket) {
    if (!socket) return;

    console.error('[%d] got socket', id, m);

    // will call .end('end') or .write('write');
    socket[m](m);

    socket.resume();

    socket.on('data', function() {
      console.error('[%d] socket.data', id, m);
    });

    socket.on('end', function() {
      console.error('[%d] socket.end', id, m);
    });

    // store the unfinished socket
    if (m === 'write') {
      needEnd.push(socket);
    }

    socket.on('close', function(had_error) {
      console.error('[%d] socket.close', id, had_error, m);
    });

    socket.on('finish', function() {
      console.error('[%d] socket finished', id, m);
    });
  });

  process.on('message', function(m) {
    if (m !== 'close') return;
    console.error('[%d] got close message', id);
    needEnd.forEach(function(endMe, i) {
      console.error('[%d] ending %d/%d', id, i, needEnd.length);
      endMe.end('end');
    });
  });

  process.on('disconnect', function() {
    console.error('[%d] process disconnect, ending', id);
    needEnd.forEach(function(endMe, i) {
      console.error('[%d] ending %d/%d', id, i, needEnd.length);
      endMe.end('end');
    });
  });

} else {

  var child1 = fork(process.argv[1], ['child', '1']);
  var child2 = fork(process.argv[1], ['child', '2']);
  var child3 = fork(process.argv[1], ['child', '3']);

  var server = net.createServer();

  var connected = 0,
      closed = 0;
  server.on('connection', function(socket) {
    switch (connected % 6) {
      case 0:
        child1.send('end', socket, { track: false }); break;
      case 1:
        child1.send('write', socket, { track: true }); break;
      case 2:
        child2.send('end', socket, { track: true }); break;
      case 3:
        child2.send('write', socket, { track: false }); break;
      case 4:
        child3.send('end', socket, { track: false }); break;
      case 5:
        child3.send('write', socket, { track: false }); break;
    }
    connected += 1;

    socket.once('close', function() {
      console.log('[m] socket closed, total %d', ++closed);
    });

    if (connected === count) {
      closeServer();
    }
  });

  var disconnected = 0;
  server.on('listening', function() {

    var j = count, client;
    while (j--) {
      client = net.connect(common.PORT, '127.0.0.1');
      client.on('error', function() {
        // This can happen if we kill the child too early.
        // The client should still get a close event afterwards.
        console.error('[m] CLIENT: error event');
      });
      client.on('close', function() {
        console.error('[m] CLIENT: close event');
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
    console.error('[m] server close');
    closeEmitted = true;

    console.error('[m] killing child processes');
    child1.kill();
    child2.kill();
    child3.kill();
  });

  server.listen(common.PORT, '127.0.0.1');

  var timeElapsed = 0;
  var closeServer = function() {
    console.error('[m] closeServer');
    var startTime = Date.now();
    server.on('close', function() {
      console.error('[m] emit(close)');
      timeElapsed = Date.now() - startTime;
    });

    console.error('[m] calling server.close');
    server.close();

    setTimeout(function() {
      assert(!closeEmitted);
      console.error('[m] sending close to children');
      child1.send('close');
      child2.send('close');
      child3.disconnect();
    }, 200);
  };

  var min = 190;
  var max = common.platformTimeout(2000);
  process.on('exit', function() {
    assert.equal(disconnected, count);
    assert.equal(connected, count);
    assert.ok(closeEmitted);
    assert.ok(timeElapsed >= min && timeElapsed <= max,
              `timeElapsed was not between ${min} and ${max} ms:` +
              `${timeElapsed}`);
  });
}
