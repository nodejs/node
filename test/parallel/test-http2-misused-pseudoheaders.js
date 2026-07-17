'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

server.on('stream', common.mustCall((stream) => {
  [
    ':path',
    ':authority',
    ':method',
    ':scheme',
  ].forEach((i) => {
    assert.throws(() => stream.respond({ [i]: '/' }),
                  {
                    code: 'ERR_HTTP2_INVALID_PSEUDOHEADER'
                  });
  });

  stream.respond({}, { waitForTrailers: true });

  stream.on('wantTrailers', common.mustCall(() => {
    assert.throws(() => {
      stream.sendTrailers({ ':status': 'bar' });
    }, {
      code: 'ERR_HTTP2_INVALID_PSEUDOHEADER'
    });
    stream.close();
  }));

  stream.end('hello world');
}));


server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  // Server uses waitForTrailers, fails to send valid trailers, then
  // close()s — peer reset arrives before END_STREAM, surfacing as
  // 'error' on the client.
  req.on('response', common.mustCall());
  req.resume();
  req.on('error', () => {});
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
