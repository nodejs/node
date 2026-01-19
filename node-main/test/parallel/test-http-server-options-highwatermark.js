// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const { kHighWaterMark } = require('_http_outgoing');

const { getDefaultHighWaterMark } = require('internal/streams/state');

function listen(server) {
  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port,
    }, (res) => {
      assert.strictEqual(res.statusCode, 200);
      res.resume().on('end', common.mustCall(() => {
        server.close();
      }));
    });
  }));
}

{
  const server = http.createServer({
    highWaterMark: getDefaultHighWaterMark() * 2,
  }, common.mustCall((req, res) => {
    assert.strictEqual(req._readableState.highWaterMark, getDefaultHighWaterMark() * 2);
    assert.strictEqual(res[kHighWaterMark], getDefaultHighWaterMark() * 2);
    res.statusCode = 200;
    res.end();
  }));

  listen(server);
}

{
  const server = http.createServer(
    common.mustCall((req, res) => {
      assert.strictEqual(req._readableState.highWaterMark, getDefaultHighWaterMark());
      assert.strictEqual(res[kHighWaterMark], getDefaultHighWaterMark());
      res.statusCode = 200;
      res.end();
    })
  );

  listen(server);
}
