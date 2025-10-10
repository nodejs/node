'use strict';
const common = require('../common');
const assert = require('assert');
const {
  executionAsyncResource,
  executionAsyncId,
  createHook,
} = require('async_hooks');
const http = require('http');

const hooked = {};
createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    hooked[asyncId] = resource;
  },
}).enable();

const server = http.createServer((req, res) => {
  res.write('hello');
  setTimeout(() => {
    res.end(' world!');
  }, 1000);
});

server.listen(0, common.mustCallAtLeast(() => {
  assert.strictEqual(executionAsyncResource(), hooked[executionAsyncId()]);
  http.get({ port: server.address().port }, common.mustCallAtLeast((res) => {
    assert.strictEqual(executionAsyncResource(), hooked[executionAsyncId()]);
    res.on('data', common.mustCallAtLeast(() => {
      assert.strictEqual(executionAsyncResource(), hooked[executionAsyncId()]);
    }));
    res.on('end', common.mustCall(() => {
      assert.strictEqual(executionAsyncResource(), hooked[executionAsyncId()]);
      server.close();
    }));
  }));
}));
