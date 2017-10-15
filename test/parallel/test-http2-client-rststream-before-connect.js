'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':path': '/' });
  // make sure that destroy is called twice
  req._destroy = common.mustCall(req._destroy.bind(req), 2);

  client.rstStream(req, 0);
  // redundant call to rstStream with new code
  client.rstStream(req, 1);
  // confirm that rstCode is from the first call to rstStream
  assert.strictEqual(req.rstCode, 0);

  req.on('streamClosed', common.mustCall((code) => {
    assert.strictEqual(req.destroyed, true);
    assert.strictEqual(code, 0);
    server.close();
    client.destroy();
  }));

  req.on('response', common.mustNotCall());
  req.resume();
  req.on('end', common.mustCall());
  req.end();
}));
