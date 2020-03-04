'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');
const { AsyncLocalStorage } = require('async_hooks');
const als = new AsyncLocalStorage();

const maxSize = 1024;

const server = http.createServer(common.mustCall((req, res) => {
  server.close();

  res.writeHead(200, { 'Content-Type': 'text/plain' });
  for (let i = 0; i < maxSize; i++) {
    res.write(`x${i}`);
  }
  res.end();
}));

server.listen(0, () => {
  als.run(new Map(), common.mustCall(() => {
    const options = { port: server.address().port };
    const req = http.get(options, common.mustCall((res) => {
      const store = als.getStore();
      store.set('req', req);
      store.set('size', 0);
      res.on('data', ondata);
      req.on('abort', common.mustCall(onabort));
      assert.strictEqual(req.aborted, false);
    }));
  }));
});

function ondata(d) {
  const store = als.getStore();
  const req = store.get('req');
  let size = store.get('size');
  size += d.length;
  assert(!req.aborted, 'got data after abort');
  if (size > maxSize) {
    req.abort();
    assert.strictEqual(req.aborted, true);
    size = maxSize;
  }
  store.set('size', size);
}

function onabort() {
  const store = als.getStore();
  const size = store.get('size');
  assert.strictEqual(size, maxSize);
}
