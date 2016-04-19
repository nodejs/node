'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => {
  server.close();

  res.writeHead(200);
  res.flushHeaders();

  req.setTimeout(200, () => {
    assert(false, 'Should not happen');
  });
  req.resume();
  req.once('end', () => {
    res.end();
  });
});

server.listen(common.PORT, () => {
  const req = http.request({
    port: common.PORT,
    method: 'POST'
  }, (res) => {
    const interval = setInterval(() => {
      req.write('a');
    }, 25);
    setTimeout(() => {
      clearInterval(interval);
      req.end();
    }, 400);
  });
  req.write('.');
});
