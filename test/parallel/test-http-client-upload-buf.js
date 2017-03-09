'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const N = 1024;

const server = http.createServer(common.mustCall(function(req, res) {
  assert.equal('POST', req.method);

  let bytesReceived = 0;

  req.on('data', function(chunk) {
    bytesReceived += chunk.length;
  });

  req.on('end', common.mustCall(function() {
    assert.strictEqual(N, bytesReceived);
    console.log('request complete from server');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.write('hello\n');
    res.end();
  }));
}));
server.listen(0);

server.on('listening', common.mustCall(function() {
  const req = http.request({
    port: this.address().port,
    method: 'POST',
    path: '/'
  }, common.mustCall(function(res) {
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      console.log(chunk);
    });
    res.on('end', common.mustCall(function() {
      server.close();
    }));
  }));

  req.write(Buffer.allocUnsafe(N));
  req.end();
}));
