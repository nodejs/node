'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const HTTP_AGENT_TIMEOUT = 1000;
const HTTP_CLIENT_TIMEOUT = 2000;

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
  // never response
});

server.listen(0, options.host, function() {
  doRequest();
});

function doRequest() {
  options.port = server.address().port;
  const req = http.request(options);
  const start = Date.now();
  req.on('error', () => {
    // this space is intentionally left blank
  });
  req.on('close', common.mustCall(() => server.close()));

  let timeout_events = 0;
  req.on('timeout', common.mustCall(() => {
    timeout_events += 1;
    const duration = Date.now() - start;
    assert((Math.abs(duration - HTTP_CLIENT_TIMEOUT) < 20));
  }));
  req.end();

  setTimeout(function() {
    req.destroy();
    assert.strictEqual(timeout_events, 1);
  }, common.platformTimeout(HTTP_CLIENT_TIMEOUT + 50));
}
