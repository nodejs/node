'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();

assert.strictEqual(server.listening, false);

server.listen(0, common.mustCall(() => {
  assert.strictEqual(server.listening, true);

  server.close(common.mustCall(() => {
    assert.strictEqual(server.listening, false);
  }));
}));
