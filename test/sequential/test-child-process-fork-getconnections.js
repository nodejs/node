'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');
const count = 12;

if (process.argv[2] === 'child') {
  const sockets = [];

  process.on('message', function(m, socket) {
    function sendClosed(id) {
      process.send({ id: id, status: 'closed'});
    }

    if (m.cmd === 'new') {
      assert(socket);
      assert(socket instanceof net.Socket, 'should be a net.Socket');
      sockets.push(socket);
    }

    if (m.cmd === 'close') {
      assert.equal(socket, undefined);
      if (sockets[m.id].destroyed) {
        // Workaround for https://github.com/nodejs/node/issues/2610
        sendClosed(m.id);
        // End of workaround. When bug is fixed, this code can be used instead:
        // throw new Error('socket destroyed unexpectedly!');
      } else {
        sockets[m.id].once('close', sendClosed.bind(null, m.id));
        sockets[m.id].destroy();
      }
    }
  });

} else {
  const child = fork(process.argv[1], ['child']);

  child.on('exit', function(code, signal) {
    if (!childKilled)
      throw new Error('child died unexpectedly!');
  });

  const server = net.createServer();
  const sockets = [];
  let sent = 0;

  server.on('connection', function(socket) {
    child.send({ cmd: 'new' }, socket);
    sockets.push(socket);

    if (sockets.length === count) {
      closeSockets(0);
    }
  });

  let disconnected = 0;
  server.on('listening', function() {
    let j = count;
    while (j--) {
      const client = net.connect(common.PORT, '127.0.0.1');
      client.on('close', function() {
        disconnected += 1;
      });
    }
  });

  let childKilled = false;
  function closeSockets(i) {
    if (i === count) {
      childKilled = true;
      server.close();
      child.kill();
      return;
    }

    child.once('message', function(m) {
      assert(m.status === 'closed');
      server.getConnections(function(err, num) {
        closeSockets(i + 1);
      });
    });
    sent++;
    child.send({ id: i, cmd: 'close' });
  }

  let closeEmitted = false;
  server.on('close', function() {
    closeEmitted = true;
  });

  server.listen(common.PORT, '127.0.0.1');

  process.on('exit', function() {
    assert.equal(sent, count);
    assert.equal(disconnected, count);
    assert.ok(closeEmitted);
    console.log('ok');
  });
}
