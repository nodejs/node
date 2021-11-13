'use strict';

require('../common');

// Regression tests for https://github.com/nodejs/node/issues/40693

const assert = require('assert');
const net = require('net');
const { AsyncLocalStorage } = require('async_hooks');

net
  .createServer((socket) => {
    socket.write('Hello, world!');
    socket.pipe(socket);
  })
  .listen(0, function() {
    const asyncLocalStorage = new AsyncLocalStorage();
    const store = { val: 'abcd' };
    asyncLocalStorage.run(store, () => {
      const client = net.connect({ port: this.address().port });
      client.on('data', () => {
        assert.deepStrictEqual(asyncLocalStorage.getStore(), store);
        client.end();
        this.close();
      });
    });
  });
