'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { once } = require('events');
const { connect } = require('net/promises');

(async () => {
  // Resolves with a connected socket and round-trips data.
  {
    const server = net.createServer((socket) => {
      socket.end('hello');
    }).listen(0);
    await once(server, 'listening');
    const socket = await connect({ port: server.address().port });
    assert.strictEqual(socket.connecting, false);
    const chunks = [];
    for await (const chunk of socket) {
      chunks.push(chunk);
    }
    assert.strictEqual(Buffer.concat(chunks).toString(), 'hello');
    server.close();
  }

  // net.promises is the same object as require('net/promises').
  assert.strictEqual(net.promises, require('net/promises'));

  // Rejects when the connection is refused.
  {
    const server = net.createServer().listen(0);
    await once(server, 'listening');
    const { port } = server.address();
    server.close();
    await once(server, 'close');
    await assert.rejects(connect({ port }), { code: 'ECONNREFUSED' });
  }

  // A pre-aborted signal rejects with an AbortError.
  {
    await assert.rejects(
      connect({ port: 0, signal: AbortSignal.abort() }),
      { name: 'AbortError' });
  }

  // Aborting while connecting rejects with an AbortError.
  {
    const server = net.createServer().listen(0);
    await once(server, 'listening');
    const controller = new AbortController();
    const promise = connect({ port: server.address().port, signal: controller.signal });
    controller.abort();
    await assert.rejects(promise, { name: 'AbortError' });
    server.close();
  }

  // An invalid signal throws.
  {
    await assert.rejects(
      connect({ port: 0, signal: 'INVALID_SIGNAL' }),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }
})().then(common.mustCall());
