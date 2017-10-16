'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

server.on(
  'stream',
  common.mustCall((stream) => {
    const errorObj = {
      type: Error,
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: 'The stream has been destroyed'
    };
    stream.session.destroy();

    // Test that stream.state getter returns an empty object
    // when the stream session has been destroyed
    assert.deepStrictEqual(Object.create(null), stream.state);

    // Test that ERR_HTTP2_INVALID_STREAM is thrown while calling
    // stream operations after the stream session has been destroyed
    common.expectsError(() => stream.additionalHeaders(), errorObj);
    common.expectsError(() => stream.priority(), errorObj);
    common.expectsError(
      () => stream.pushStream({}, common.mustNotCall()),
      errorObj
    );
    common.expectsError(() => stream.respond(), errorObj);
    common.expectsError(() => stream.rstStream(), errorObj);
    common.expectsError(() => stream.write('data'), errorObj);
  })
);

server.listen(
  0,
  common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => server.close()));
  })
);
