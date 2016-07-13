'use strict';

const common = require('../common');
const http = require('http');

const server = http.createServer((req, res) => {
  server.close();

  res.writeHead(200);
  res.flushHeaders();

  req.setTimeout(common.platformTimeout(200), () => {
    common.fail('Request timeout should not fire');
  });
  req.resume();
  req.once('end', common.mustCall(() => {
    res.end();
  }));
});

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'POST'
  }, (res) => {
    const interval = setInterval(() => {
      req.write('a');
    }, common.platformTimeout(25));
    setTimeout(() => {
      clearInterval(interval);
      req.end();
    }, common.platformTimeout(200));
  });
  req.write('.');
}));
