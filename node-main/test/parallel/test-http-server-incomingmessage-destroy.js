'use strict';

const common = require('../common');
const { createServer, get } = require('http');
const assert = require('assert');

const server = createServer(common.mustCall((req, res) => {
  req.destroy(new Error('Destroy test'));
}));

function onUncaught(error) {}

process.on('uncaughtException', common.mustNotCall(onUncaught));

server.listen(0, common.mustCall(() => {
  get({
    port: server.address().port
  }, (res) => {
    res.resume();
  }).on('error', (error) => {
    assert.strictEqual(error.message, 'socket hang up');
    assert.strictEqual(error.code, 'ECONNRESET');
    server.close();
  });
}));
