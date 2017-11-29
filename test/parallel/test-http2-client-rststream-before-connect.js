'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();
server.on('stream', (stream) => {
  stream.respond();
  stream.end('ok');
});

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':path': '/' });
  req.rstStream(0);

  // make sure that destroy is called
  req._destroy = common.mustCall(req._destroy.bind(req));

  // second call doesn't do anything
  assert.doesNotThrow(() => req.rstStream(8));

  req.on('close', common.mustCall((code) => {
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
