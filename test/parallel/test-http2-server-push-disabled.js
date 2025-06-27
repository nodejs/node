'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('session', common.mustCall((session) => {
  // Verify that the settings disabling push is received
  session.on('remoteSettings', common.mustCall((settings) => {
    assert.strictEqual(settings.enablePush, false);
  }));
}));

server.on('stream', common.mustCall((stream) => {

  // The client has disabled push streams, so pushAllowed must be false,
  // and pushStream() must throw.
  assert.strictEqual(stream.pushAllowed, false);

  assert.throws(() => {
    stream.pushStream({
      ':scheme': 'http',
      ':path': '/foobar',
      ':authority': `localhost:${server.address().port}`,
    }, common.mustNotCall());
  }, {
    code: 'ERR_HTTP2_PUSH_DISABLED',
    name: 'Error'
  });

  stream.respond({ ':status': 200 });
  stream.end('test');
}));

server.listen(0, common.mustCall(() => {
  const options = { settings: { enablePush: false } };
  const client = http2.connect(`http://localhost:${server.address().port}`,
                               options);
  const req = client.request({ ':path': '/' });

  // Because push streams are disabled, this must not be called.
  client.on('stream', common.mustNotCall());

  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
  req.end();
}));
