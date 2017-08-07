// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const checkWeight = (actual, expect) => {
  const server = http2.createServer();
  server.on('stream', common.mustCall((stream, headers, flags) => {
    assert.strictEqual(stream.state.weight, expect);
    stream.respond({
      'content-type': 'text/html',
      ':status': 200
    });
    stream.end('test');
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http2.connect(`http://localhost:${port}`);

    const headers = { ':path': '/' };
    const req = client.request(headers, { weight: actual });

    req.on('data', common.mustCall(() => {}));
    req.on('end', common.mustCall(() => {
      server.close();
      client.destroy();
    }));
    req.end();
  }));
};

// when client weight is lower than 1, weight is 1
checkWeight(-1, 1);
checkWeight(0, 1);

// 1 - 256 is correct weight
checkWeight(1, 1);
checkWeight(16, 16);
checkWeight(256, 256);

// when client weight is higher than 256, weight is 256
checkWeight(257, 256);
checkWeight(512, 256);

// when client weight is undefined, weight is default 16
checkWeight(undefined, 16);
