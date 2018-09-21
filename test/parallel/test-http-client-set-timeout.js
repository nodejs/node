'use strict';
const common = require('../common');

// Test that `req.setTimeout` will fired exactly once.

const assert = require('assert');
const http = require('http');

const HTTP_CLIENT_TIMEOUT = 2000;

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/',
  timeout: HTTP_CLIENT_TIMEOUT,
};

const server = http.createServer(() => {
  // Never respond.
});

server.listen(0, options.host, function() {
  doRequest();
});

function doRequest() {
  options.port = server.address().port;
  const req = http.request(options);
  req.setTimeout(HTTP_CLIENT_TIMEOUT / 2);
  req.on('error', () => {
    // This space is intentionally left blank.
  });
  req.on('close', common.mustCall(() => server.close()));

  let timeout_events = 0;
  req.on('timeout', common.mustCall(() => {
    timeout_events += 1;
  }));
  req.end();

  setTimeout(function() {
    req.destroy();
    assert.strictEqual(timeout_events, 1);
  }, common.platformTimeout(HTTP_CLIENT_TIMEOUT));
}
