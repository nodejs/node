'use strict';
require('../common');
const assert = require('assert');

const http = require('http');

let serverRequests = 0;
let clientRequests = 0;

const server = http.createServer(function(req, res) {
  serverRequests++;
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('OK');
});

server.listen(0, function() {
  function callback() {}

  const req = http.request({
    port: this.address().port,
    path: '/',
    agent: false
  }, function(res) {
    req.clearTimeout(callback);

    res.on('end', function() {
      clientRequests++;
      server.close();
    });

    res.resume();
  });

  // Overflow signed int32
  req.setTimeout(0xffffffff, callback);
  req.end();
});

process.once('exit', function() {
  assert.strictEqual(clientRequests, 1);
  assert.strictEqual(serverRequests, 1);
});
