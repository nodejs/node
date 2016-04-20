'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  server.close();

  res.writeHead(200);
  res.flushHeaders();

  req.setTimeout(common.platformTimeout(200), () => {
    assert(false, 'Should not happen');
  });
  req.resume();
  req.once('end', common.mustCall(() => {
    res.end();
  }));
});

server.listen(common.PORT, common.mustCall(() => {
  const req = http.request({
    port: common.PORT,
    method: 'POST'
  }, (res) => {
    const interval = setInterval(() => {
      req.write('a');
    }, common.platformTimeout(25));
    setTimeout(() => {
      clearInterval(interval);
      req.end();
    }, common.platformTimeout(400));
  });
  req.write('.');
}));
