'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

common.expectWarning(
  'DeprecationWarning',
  'Priority signaling has been deprecated as of RFC 9113.',
  'DEP0194');

const checkWeight = (actual, expect) => {
  const server = http2.createServer();
  server.on('stream', common.mustCall((stream, headers, flags) => {
    assert.strictEqual(stream.state.sumDependencyWeight, 0);
    assert.strictEqual(stream.state.weight, 16);
    stream.respond();
    stream.end('test');
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({}, { weight: actual });

    req.on('data', common.mustCall());
    req.on('end', common.mustCall());
    req.on('close', common.mustCall(() => {
      server.close();
      client.close();
    }));
  }));
};

// When client weight is lower than 1, weight is 1
checkWeight(-1, 1);
checkWeight(0, 1);

// 1 - 256 is correct weight
checkWeight(1, 1);
checkWeight(16, 16);
checkWeight(256, 256);

// When client weight is higher than 256, weight is 256
checkWeight(257, 256);
checkWeight(512, 256);

// When client weight is undefined, weight is default 16
checkWeight(undefined, 16);
