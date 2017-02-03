'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server1 = http.createServer(common.mustNotCall());
server1.listen(0, '127.0.0.1', common.mustCall(function() {
  const server2 = http.createServer(common.mustNotCall());
  server2.listen(this.address().port, '127.0.0.1', common.mustNotCall());

  server2.on('error', common.mustCall(function(e) {
    assert.strictEqual(e.code, 'EADDRINUSE');
    server1.close();
  }));
}));
