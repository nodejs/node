'use strict';
const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { kConnectionsCheckingInterval } = require('_http_server');

const server = createServer(function(req, res) {});
server.listen(0, common.mustCall(function() {
  assert.strictEqual(server[kConnectionsCheckingInterval]._destroyed, false);
  server.close(common.mustCall(() => {
    assert(server[kConnectionsCheckingInterval]._destroyed);
  }));
}));
