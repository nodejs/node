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
    res.destroy(new Error('Destroy test'));
  }));
});
