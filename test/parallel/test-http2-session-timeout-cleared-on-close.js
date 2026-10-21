'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// A destroyed session must not keep its idle timer armed: setTimeout() is a
// no-op once the session is marked destroyed, so closeSession() has to clear
// the timer first.
const server = http2.createServer();
server.setTimeout(30_000, common.mustNotCall());

server.on('session', common.mustCall((session) => {
  session.on('close', common.mustCall(() => {
    assert.strictEqual(session.timeout, 0);
    server.close();
  }));
}));

server.on('stream', common.mustCall((stream) => {
  stream.respond({ ':status': 200 });
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();
  req.on('end', common.mustCall(() => client.close()));
}));
