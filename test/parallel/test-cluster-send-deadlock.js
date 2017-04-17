'use strict';
// Testing mutual send of handles: from master to worker, and from worker to
// master.

require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  const worker = cluster.fork();
  worker.on('exit', function(code, signal) {
    assert.strictEqual(code, 0, 'Worker exited with an error code');
    assert(!signal, 'Worker exited by a signal');
    server.close();
  });

  let server = net.createServer(function(socket) {
    worker.send('handle', socket);
  });

  server.listen(0, function() {
    worker.send({message: 'listen', port: server.address().port});
  });
} else {
  process.on('message', function(msg, handle) {
    if (msg.message && msg.message === 'listen') {
      assert(msg.port);
      const client1 = net.connect({ host: 'localhost', port: msg.port });
      const client2 = net.connect({ host: 'localhost', port: msg.port });
      let waiting = 2;
      client1.on('close', onclose);
      client2.on('close', onclose);
      function onclose() {
        if (--waiting === 0)
          cluster.worker.disconnect();
      }
      setTimeout(function() {
        client1.end();
        client2.end();
      }, 50);
    } else {
      process.send('reply', handle);
    }
  });
}
