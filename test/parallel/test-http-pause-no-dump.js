'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  req.once('data', common.mustCall(() => {
    req.pause();
    res.writeHead(200);
    res.end();
    res.on('finish', common.mustCall(() => {
      assert(!req._dumped);
    }));
  }));
}));
server.listen(0);

server.on('listening', common.mustCall(function() {
  const req = http.request({
    port: this.address().port,
    method: 'POST',
    path: '/'
  }, common.mustCall(function(res) {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', common.mustCall(() => {
      server.close();
    }));
  }));

  req.end(Buffer.allocUnsafe(1024));
}));
