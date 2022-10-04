'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const { AsyncLocalStorage } = require('async_hooks');
const { createServer, get } = require('http');
const assert = require('assert');

const store = new AsyncLocalStorage();

const channel = dc.storageChannel('http.server');

// Test optional build function behaviour
channel.bindStore(store);

assert.strictEqual(store.getStore(), undefined);
const app = createServer(common.mustCall((req, res) => {
  const data = store.getStore();
  assert.ok(data);

  // Verify context data was passed through
  const { request, response, server, socket } = data;
  assert.deepStrictEqual(request, req);
  assert.deepStrictEqual(response, res);
  assert.deepStrictEqual(socket, req.socket);
  assert.deepStrictEqual(server, app);

  res.end();
  app.close();
}));
assert.strictEqual(store.getStore(), undefined);

app.listen(() => {
  const { port } = app.address();
  assert.strictEqual(store.getStore(), undefined);
  get(`http://localhost:${port}`, (res) => res.resume());
  assert.strictEqual(store.getStore(), undefined);
});
