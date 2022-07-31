'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end();
});

server.listen(0, common.mustCall(() => {
  const req = http.get({ port: server.address().port }, (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    server.close();
  });

  req.end();
}));

// This timer should never go off as the server will close the socket
setTimeout(common.mustNotCall(), common.platformTimeout(1000)).unref();
