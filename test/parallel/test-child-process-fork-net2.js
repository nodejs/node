'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');
const count = 12;

if (process.argv[2] === 'child') {
  const needEnd = [];
  const id = process.argv[3];

  process.on('message', function(m, socket) {
    if (!socket) return;

    console.error(`[${id}] got socket ${m}`);

    // will call .end('end') or .write('write');
    socket[m](m);

    socket.resume();

    socket.on('data', function() {
      console.error(`[${id}] socket.data ${m}`);
    });

    socket.on('end', function() {
      console.error(`[${id}] socket.end ${m}`);
    });

    // store the unfinished socket
    if (m === 'write') {
      needEnd.push(socket);
    }

    socket.on('close', function(had_error) {
      console.error(`[${id}] socket.close ${had_error} ${m}`);
    });

    socket.on('finish', function() {
      console.error(`[${id}] socket finished ${m}`);
    });
  });

  process.on('message', function(m) {
    if (m !== 'close') return;
    console.error(`[${id}] got close message`);
    needEnd.forEach(function(endMe, i) {
      console.error(`[${id}] ending ${i}/${needEnd.length}`);
      endMe.end('end');
    });
  });

  process.on('disconnect', function() {
    console.error(`[${id}] process disconnect, ending`);
    needEnd.forEach(function(endMe, i) {
      console.error(`[${id}] ending ${i}/${needEnd.length}`);
      endMe.end('end');
    });
  });

} else {

  const child1 = fork(process.argv[1], ['child', '1']);
  const child2 = fork(process.argv[1], ['child', '2']);
  const child3 = fork(process.argv[1], ['child', '3']);

  const server = net.createServer();

  let connected = 0;
  let closed = 0;
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

    socket.once('close', function() {
      console.log(`[m] socket closed, total ${++closed}`);
    });

    if (connected === count) {
      closeServer();
    }
  });

  let disconnected = 0;
  server.on('listening', function() {

    let j = count, client;
    while (j--) {
      client = net.connect(this.address().port, '127.0.0.1');
      client.on('error', function() {
        // This can happen if we kill the child too early.
        // The client should still get a close event afterwards.
        console.error('[m] CLIENT: error event');
      });
      client.on('close', function() {
        console.error('[m] CLIENT: close event');
        disconnected += 1;
      });
    }
  });

  let closeEmitted = false;
  server.on('close', common.mustCall(function() {
    closeEmitted = true;

    child1.kill();
    child2.kill();
    child3.kill();
  }));

  server.listen(0, '127.0.0.1');

  function closeServer() {
    server.close();

    setTimeout(function() {
      assert(!closeEmitted);
      child1.send('close');
      child2.send('close');
      child3.disconnect();
    }, 200);
  }

  process.on('exit', function() {
    assert.strictEqual(disconnected, count);
    assert.strictEqual(connected, count);
  });
}
