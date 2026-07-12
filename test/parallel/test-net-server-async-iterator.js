'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { once } = require('events');

(async () => {
  // `for await...of` yields each incoming connection as a net.Socket.
  {
    const server = net.createServer().listen(0);
    await once(server, 'listening');
    const { port } = server.address();

    const total = 3;
    const clients = [];
    for (let i = 0; i < total; i++) {
      clients.push(net.connect(port));
    }

    let count = 0;
    for await (const socket of server) {
      assert.ok(socket instanceof net.Socket);
      socket.end();
      if (++count === total) break;
    }
    assert.strictEqual(count, total);

    // Breaking out of the loop stops iterating but does not close the server.
    assert.strictEqual(server.listening, true);

    server.close();
    for (const client of clients) {
      client.destroy();
    }
  }

  // Iteration ends cleanly when the server closes.
  {
    const server = net.createServer().listen(0);
    await once(server, 'listening');
    process.nextTick(() => server.close());

    let count = 0;
    for await (const socket of server) {
      count++;
      socket.end();
    }
    assert.strictEqual(count, 0);
  }
})().then(common.mustCall());
