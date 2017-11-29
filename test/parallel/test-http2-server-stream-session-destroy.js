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
    stream.session.destroy();

    // Test that stream.state getter returns an empty object
    // when the stream session has been destroyed
    assert.deepStrictEqual({}, stream.state);

    // Test that ERR_HTTP2_INVALID_STREAM is thrown while calling
    // stream operations after the stream session has been destroyed
    const invalidStreamError = {
      type: Error,
      code: 'ERR_HTTP2_INVALID_STREAM',
      message: 'The stream has been destroyed'
    };
    common.expectsError(() => stream.additionalHeaders(), invalidStreamError);
    common.expectsError(() => stream.priority(), invalidStreamError);
    common.expectsError(
      () => stream.pushStream({}, common.mustNotCall()),
      invalidStreamError
    );
    common.expectsError(() => stream.respond(), invalidStreamError);
    common.expectsError(() => stream.write('data'), invalidStreamError);

    // Test that ERR_HTTP2_INVALID_SESSION is thrown while calling
    // session operations after the stream session has been destroyed
    const invalidSessionError = {
      type: Error,
      code: 'ERR_HTTP2_INVALID_SESSION',
      message: 'The session has been destroyed'
    };
    common.expectsError(() => stream.session.settings(), invalidSessionError);
    common.expectsError(() => stream.session.shutdown(), invalidSessionError);

    // Wait for setImmediate call from destroy() to complete
    // so that state.destroyed is set to true
    setImmediate((session) => {
      common.expectsError(() => session.settings(), invalidSessionError);
      common.expectsError(() => session.shutdown(), invalidSessionError);
    }, stream.session);
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
