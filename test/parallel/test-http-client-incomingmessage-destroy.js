'use strict';

const common = require('../common');
const { createServer, get } = require('http');
const assert = require('assert');

const server = createServer(common.mustCall((req, res) => {
  res.writeHead(200);
  res.write('Part of res.');
}));

function onUncaught(error) {
  assert.strictEqual(error.message, 'Destroy test');
  server.close();
}

process.on('uncaughtException', common.mustCall(onUncaught));

server.listen(0, () => {
  get({
    port: server.address().port
  }, common.mustCall((res) => {
    const err = new Error('Destroy test');
    assert.strictEqual(res.errored, null);
    res.destroy(err);
    assert.strictEqual(res.closed, false);
    assert.strictEqual(res.errored, err);
    res.on('close', () => {
      assert.strictEqual(res.closed, true);
    });
  }));
});
