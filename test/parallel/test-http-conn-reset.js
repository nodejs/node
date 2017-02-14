'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const options = {
  host: '127.0.0.1',
  port: undefined
};

// start a tcp server that closes incoming connections immediately
const server = net.createServer(function(client) {
  client.destroy();
  server.close();
});
server.listen(0, options.host, common.mustCall(onListen));

// do a GET request, expect it to fail
function onListen() {
  options.port = this.address().port;
  const req = http.request(options, common.mustNotCall());
  req.on('error', common.mustCall(function(err) {
    assert.strictEqual(err.code, 'ECONNRESET');
  }));
  req.end();
}
