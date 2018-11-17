'use strict';
const common = require('../common');

// Test that when http request uses both timeout and agent,
// timeout will work as expected.

const assert = require('assert');
const http = require('http');

const HTTP_AGENT_TIMEOUT = 1000;
const HTTP_CLIENT_TIMEOUT = 3000;

const agent = new http.Agent({ timeout: HTTP_AGENT_TIMEOUT });
const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/',
  timeout: HTTP_CLIENT_TIMEOUT,
  agent,
};

const server = http.createServer(() => {
  // Never respond.
});

server.listen(0, options.host, () => {
  doRequest();
});

function doRequest() {
  options.port = server.address().port;
  const req = http.request(options);
  const start = Date.now();
  req.on('error', () => {
    // This space is intentionally left blank.
  });
  req.on('close', common.mustCall(() => server.close()));

  let timeout_events = 0;
  req.on('timeout', common.mustCall(() => {
    timeout_events += 1;
    const duration = Date.now() - start;
    // The timeout event cannot be precisely timed. It will delay
    // some number of milliseconds.
    assert.ok(
      duration >= HTTP_CLIENT_TIMEOUT,
      `duration ${duration}ms less than timeout ${HTTP_CLIENT_TIMEOUT}ms`
    );
  }));
  req.end();

  setTimeout(() => {
    req.destroy();
    assert.strictEqual(timeout_events, 1);
    // Ensure the `timeout` event fired only once.
  }, common.platformTimeout(HTTP_CLIENT_TIMEOUT * 2));
}
