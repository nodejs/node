'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }
const assert = require('assert');
const http2 = require('http2');

const server1 = http2.createServer();

server1.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server1.address().port}`);
  // Check for req headers
  assert.throws(() => {
    session.request({ 'no underscore': 123 });
  }, {
    code: 'ERR_INVALID_HTTP_TOKEN'
  });
  session.close();
  server1.close();
}));

const server2 = http2.createServer(common.mustCall((req, res) => {
  // check for setHeader
  assert.throws(() => {
    res.setHeader('x y z', 123);
  }, {
    code: 'ERR_INVALID_HTTP_TOKEN'
  });
  res.end();
}));

server2.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server2.address().port}`);
  const req = session.request();
  req.on('end', common.mustCall(() => {
    session.close();
    server2.close();
  }));
}));

const server3 = http2.createServer(common.mustCall((req, res) => {
  // check for writeHead
  assert.throws(common.mustCall(() => {
    res.writeHead(200, {
      'an invalid header': 123
    });
  }), {
    code: 'ERR_INVALID_HTTP_TOKEN'
  });
  res.end();
}));

server3.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server3.address().port}`);
  const req = session.request();
  req.on('end', common.mustCall(() => {
    server3.close();
    session.close();
  }));
}));
