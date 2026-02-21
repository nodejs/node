'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  stream.setTimeout(1, common.mustCall(() => {
    stream.respond({ ':status': 200 });
    stream.end('hello world');
  }));

  // Check that expected errors are thrown with wrong args
  assert.throws(
    () => stream.setTimeout('100'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message:
        'The "msecs" argument must be of type number. Received type string' +
        " ('100')"
    }
  );
  assert.throws(
    () => stream.setTimeout(0, Symbol('test')),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
  assert.throws(
    () => stream.setTimeout(100, {}),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
}));
server.listen(0);

server.on('listening', common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  client.setTimeout(1, common.mustCall(() => {
    const req = client.request({ ':path': '/' });
    req.setTimeout(1, common.mustCall(() => {
      req.on('response', common.mustCall());
      req.resume();
      req.on('end', common.mustCall(() => {
        server.close();
        client.close();
      }));
      req.end();
    }));
  }));
}));
