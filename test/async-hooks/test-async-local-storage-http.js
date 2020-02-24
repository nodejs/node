'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const http = require('http');

const asyncLocalStorage = new AsyncLocalStorage();
const server = http.createServer((req, res) => {
  res.end('ok');
});

server.listen(0, () => {
  asyncLocalStorage.run(new Map(), () => {
    const store = asyncLocalStorage.getStore();
    store.set('hello', 'world');
    http.get({ host: 'localhost', port: server.address().port }, () => {
      assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'world');
      server.close();
    });
  });
});
