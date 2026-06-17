'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const { listen } = require('net/promises');

(async () => {
  // Resolves with a listening server.
  {
    const server = await listen({ port: 0 });
    assert.strictEqual(server.listening, true);
    assert.strictEqual(typeof server.address().port, 'number');
    server.close();
  }

  // The connectionListener option receives incoming connections.
  {
    const server = await listen({
      port: 0,
      connectionListener: common.mustCall((socket) => {
        socket.end();
        server.close();
      }),
    });
    const client = net.connect(server.address().port);
    client.resume();
  }

  // A pre-aborted signal rejects with an AbortError.
  {
    await assert.rejects(
      listen({ port: 0, signal: AbortSignal.abort() }),
      { name: 'AbortError' });
  }

  // Aborting while binding rejects with an AbortError and closes the server.
  {
    const controller = new AbortController();
    const promise = listen({ port: 0, signal: controller.signal });
    controller.abort();
    await assert.rejects(promise, { name: 'AbortError' });
  }

  // An invalid signal throws.
  {
    await assert.rejects(
      listen({ port: 0, signal: 'INVALID_SIGNAL' }),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }

  // Rejects when the address is already in use.
  {
    const first = await listen({ port: 0 });
    const { port } = first.address();
    await assert.rejects(listen({ port }), { code: 'EADDRINUSE' });
    first.close();
  }
})().then(common.mustCall());
