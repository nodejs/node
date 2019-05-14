'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.Server(common.mustCall((req, res) => {
  let resClosed = false;

  res.end();
  res.on('finish', common.mustCall(() => {
    assert.strictEqual(resClosed, false);
  }));
  res.on('close', common.mustCall(() => {
    resClosed = true;
  }));
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req._readableState.ended, true);
  }));
  res.socket.on('close', () => server.close());
}));

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall());
}));
