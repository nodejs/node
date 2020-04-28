'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

const server = http.Server(common.mustCall((req, res) => {
  let resClosed = false;

  res.end();
  let resFinished = false;
  res.on('finish', common.mustCall(() => {
    resFinished = true;
    assert.strictEqual(resClosed, false);
    assert.strictEqual(res.destroyed, false);
    assert.strictEqual(resClosed, false);
  }));
  assert.strictEqual(req.destroyed, false);
  res.on('close', common.mustCall(() => {
    resClosed = true;
    assert.strictEqual(resFinished, true);
    assert.strictEqual(res.destroyed, true);
  }));
  assert.strictEqual(req.destroyed, false);
  req.on('end', common.mustCall(() => {
    assert.strictEqual(req.destroyed, false);
  }));
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.destroyed, true);
    assert.strictEqual(req._readableState.ended, true);
  }));
  res.socket.on('close', () => server.close());
}));

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall());
}));
