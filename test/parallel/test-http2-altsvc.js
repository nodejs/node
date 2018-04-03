'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const { URL } = require('url');
const Countdown = require('../common/countdown');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.session.altsvc('h2=":8000"', stream.id);
  stream.respond();
  stream.end('ok');
}));
server.on('session', common.mustCall((session) => {
  // Origin may be specified by string, URL object, or object with an
  // origin property. For string and URL object, origin is guaranteed
  // to be an ASCII serialized origin. For object with an origin
  // property, it is up to the user to ensure proper serialization.
  session.altsvc('h2=":8000"', 'https://example.org:8111/this');
  session.altsvc('h2=":8000"', new URL('https://example.org:8111/this'));
  session.altsvc('h2=":8000"', { origin: 'https://example.org:8111' });

  // Won't error, but won't send anything because the stream does not exist
  session.altsvc('h2=":8000"', 3);

  // Will error because the numeric stream id is out of valid range
  [0, -1, 1.1, 0xFFFFFFFF + 1, Infinity, -Infinity].forEach((i) => {
    common.expectsError(
      () => session.altsvc('h2=":8000"', i),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError
      }
    );
  });

  // First argument must be a string
  [0, {}, [], null, Infinity].forEach((i) => {
    common.expectsError(
      () => session.altsvc(i),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError
      }
    );
  });

  ['\u0001', 'h2="\uff20"', '👀'].forEach((i) => {
    common.expectsError(
      () => session.altsvc(i),
      {
        code: 'ERR_INVALID_CHAR',
        type: TypeError
      }
    );
  });

  [{}, [], true].forEach((i) => {
    common.expectsError(
      () => session.altsvc('clear', i),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError
      }
    );
  });

  [
    'abc:',
    new URL('abc:'),
    { origin: 'null' },
    { origin: '' }
  ].forEach((i) => {
    common.expectsError(
      () => session.altsvc('h2=":8000', i),
      {
        code: 'ERR_HTTP2_ALTSVC_INVALID_ORIGIN',
        type: TypeError
      }
    );
  });

  // arguments + origin are too long for an ALTSVC frame
  common.expectsError(
    () => {
      session.altsvc('h2=":8000"',
                     `http://example.${'a'.repeat(17000)}.org:8000`);
    },
    {
      code: 'ERR_HTTP2_ALTSVC_LENGTH',
      type: TypeError
    }
  );
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(4, () => {
    client.close();
    server.close();
  });

  client.on('altsvc', common.mustCall((alt, origin, stream) => {
    assert.strictEqual(alt, 'h2=":8000"');
    switch (stream) {
      case 0:
        assert.strictEqual(origin, 'https://example.org:8111');
        break;
      case 1:
        assert.strictEqual(origin, '');
        break;
      default:
        assert.fail('should not happen');
    }
    countdown.dec();
  }, 4));

  const req = client.request();
  req.resume();
  req.on('close', common.mustCall());
}));
