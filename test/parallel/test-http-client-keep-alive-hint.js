'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(
  { keepAliveTimeout: common.platformTimeout(60000) },
  function(req, res) {
    req.resume();
    res.writeHead(200, { 'Connection': 'keep-alive', 'Keep-Alive': 'timeout=1' });
    res.end('FOO');
  }
);

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, (res) => {
    assert.strictEqual(res.statusCode, 200);

    res.resume();
    server.close();
  });
}));


// This timer should never go off as the agent will parse the hint and terminate earlier
setTimeout(common.mustNotCall(), common.platformTimeout(3000)).unref();
