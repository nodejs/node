'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

var requests = 0;

http.createServer(function(req, res) {
  res.writeHead(200);
  res.end('ok');

  requests++;
}).listen(0, function() {
  var agent = new http.Agent();
  agent.defaultPort = this.address().port;

  // options marked as explicitly undefined for readability
  // in this test, they should STAY undefined as options should not
  // be mutable / modified
  var options = {
    host: undefined,
    hostname: common.localhostIPv4,
    port: undefined,
    defaultPort: undefined,
    path: undefined,
    method: undefined,
    agent: agent
  };

  http.request(options, function(res) {
    res.resume();
  }).end();

  process.on('exit', function() {
    assert.equal(requests, 1);

    assert.strictEqual(options.host, undefined);
    assert.strictEqual(options.hostname, common.localhostIPv4);
    assert.strictEqual(options.port, undefined);
    assert.strictEqual(options.defaultPort, undefined);
    assert.strictEqual(options.path, undefined);
    assert.strictEqual(options.method, undefined);
  });
}).unref();

