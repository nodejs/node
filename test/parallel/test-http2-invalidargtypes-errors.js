'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  assert.throws(
    () => stream.close('string'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
  assert.throws(
    () => stream.close(1.01),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    }
  );
  [-1, 2 ** 32].forEach((code) => {
    assert.throws(
      () => stream.close(code),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
      }
    );
  });
  stream.respond();
  stream.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
