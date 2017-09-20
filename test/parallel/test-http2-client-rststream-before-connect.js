// Flags: --expose-http2
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
  client.rstStream(req, 0);
  assert.strictEqual(req.rstCode, 0);

  // make sure that destroy is called
  req._destroy = common.mustCall(req._destroy.bind(req));

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
