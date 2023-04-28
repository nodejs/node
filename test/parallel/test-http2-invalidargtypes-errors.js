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
      message: 'The "code" argument must be of type number. ' +
               "Received type string ('string')"
    }
  );
  assert.throws(
    () => stream.close(1.01),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "code" is out of range. It must be an integer. ' +
               'Received 1.01'
    }
  );
  [-1, 2 ** 32].forEach((code) => {
    assert.throws(
      () => stream.close(code),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: 'The value of "code" is out of range. ' +
                 'It must be >= 0 && <= 4294967295. ' +
                 `Received ${code}`
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
