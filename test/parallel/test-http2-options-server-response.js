'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

class MyServerResponse extends h2.Http2ServerResponse {
  status(code) {
    return this.writeHead(code, { 'Content-Type': 'text/plain' });
  }

  write(...args) {
    this.writeCalled = true;
    return super.write(...args);
  }
}

const server = h2.createServer({
  Http2ServerResponse: MyServerResponse
}, common.mustCall((req, res) => {
  res.status(200);
  res.end('body');
  assert.strictEqual(res.writeCalled, true);
}));
server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':path': '/' });

  req.on('response', common.mustCall());

  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
}));
