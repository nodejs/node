'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.Server(common.mustCall((req, res) => {
  res.end();
  res.on('finish', common.mustCall(() => {
    assert.strictEqual(res.closed, false);
  }));
  res.on('close', common.mustCall(() => {
    assert.strictEqual(res.closed, true);
  }));
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.closed, true);
    assert.strictEqual(req._readableState.ended, true);
  }));
  res.socket.on('close', () => server.close());
}));

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall());
}));
