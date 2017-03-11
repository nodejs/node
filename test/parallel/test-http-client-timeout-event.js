'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

const server = http.createServer(function(req, res) {
  // this space intentionally left blank
});

server.listen(0, options.host, function() {
  options.port = this.address().port;
  const req = http.request(options, function(res) {
    // this space intentionally left blank
  });
  req.on('error', function() {
    // this space is intentionally left blank
  });
  req.on('close', function() {
    server.close();
  });

  let timeout_events = 0;
  req.setTimeout(1);
  req.on('timeout', function() {
    timeout_events += 1;
  });
  setTimeout(function() {
    req.destroy();
    assert.equal(timeout_events, 1);
  }, common.platformTimeout(100));
  setTimeout(function() {
    req.end();
  }, common.platformTimeout(50));
});
