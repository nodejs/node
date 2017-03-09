'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/',
  timeout: 1
};

const server = http.createServer();

server.listen(0, options.host, function() {
  options.port = this.address().port;
  const req = http.request(options);
  req.on('error', function() {
    // this space is intentionally left blank
  });
  req.on('close', common.mustCall(() => server.close()));

  let timeout_events = 0;
  req.on('timeout', common.mustCall(() => timeout_events += 1));
  setTimeout(function() {
    req.destroy();
    assert.strictEqual(timeout_events, 1);
  }, common.platformTimeout(100));
  setTimeout(function() {
    req.end();
  }, common.platformTimeout(10));
});
