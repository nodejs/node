'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

// Regression test for https://github.com/nodejs/node/issues/29457:
// HTTP/2 stream event listeners can be added and removed after the
// session has been destroyed.

const server = http2.createServer((req, res) => {
  res.end('Hi!\n');
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const headers = { ':path': '/' };
  const req = client.request(headers);

  req.on('close', common.mustCall(() => {
    req.removeAllListeners();
    req.on('priority', common.mustNotCall());
    server.close();
  }));

  req.on('priority', common.mustNotCall());
  req.on('error', common.mustCall());

  client.destroy();
}));
