// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

// Each of these headers must appear only once
const singles = [
  'content-type',
  'user-agent',
  'referer',
  'authorization',
  'proxy-authorization',
  'if-modified-since',
  'if-unmodified-since',
  'from',
  'location',
  'max-forwards'
];

server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  let remaining = singles.length * 2;
  function maybeClose() {
    if (--remaining === 0) {
      server.close();
      client.destroy();
    }
  }

  singles.forEach((i) => {
    const req = client.request({
      [i]: 'abc',
      [i.toUpperCase()]: 'xyz'
    });
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
      type: Error,
      message: `Header field "${i}" must have only a single value`
    }));
    req.on('error', common.mustCall(maybeClose));

    const req2 = client.request({
      [i]: ['abc', 'xyz']
    });
    req2.on('error', common.expectsError({
      code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
      type: Error,
      message: `Header field "${i}" must have only a single value`
    }));
    req2.on('error', common.mustCall(maybeClose));
  });

}));
