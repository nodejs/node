'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

var options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

var server = http.createServer();

server.listen(0, options.host, function() {
  options.port = this.address().port;
  var req = http.request(options);
  req.on('error', function() {
    // this space is intentionally left blank
  });
  req.on('close', common.mustCall(() => server.close()));

  var timeout_events = 0;
  req.setTimeout(1);
  req.on('timeout', common.mustCall(() => timeout_events += 1));
  setTimeout(function() {
    req.destroy();
    assert.strictEqual(timeout_events, 1);
  }, common.platformTimeout(100));
  setTimeout(function() {
    req.end();
  }, common.platformTimeout(50));
});
