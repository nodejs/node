'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const { HTTPParser } = require('_http_common');

// Test that the `HTTPParser` instance is cleaned up before being returned to
// the pool to avoid memory retention issues.

const kOnTimeout = HTTPParser.kOnTimeout | 0;
const server = http.createServer();

server.on('request', common.mustCall((request, response) => {
  const parser = request.socket.parser;

  assert.strictEqual(typeof parser[kOnTimeout], 'function');

  request.socket.on('close', common.mustCall(() => {
    assert.strictEqual(parser[kOnTimeout], null);
  }));

  response.end();
  server.close();
}));

server.listen(common.mustCall(() => {
  const request = http.get({
    headers: { Connection: 'close' },
    port: server.address().port,
    joinDuplicateHeaders: true
  });
  let parser;

  request.on('socket', common.mustCall(() => {
    parser = request.parser;
    assert.strictEqual(typeof parser.onIncoming, 'function');
    assert.strictEqual(parser.joinDuplicateHeaders, true);
  }));

  request.on('response', common.mustCall((response) => {
    response.resume();
    response.on('end', common.mustCall(() => {
      assert.strictEqual(parser.onIncoming, null);
      assert.strictEqual(parser.joinDuplicateHeaders, null);
    }));
  }));
}));
