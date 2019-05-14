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
  [0, -1, 1.1, 0xFFFFFFFF + 1, Infinity, -Infinity].forEach((input) => {
    assert.throws(
      () => session.altsvc('h2=":8000"', input),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: 'The value of "originOrStream" is out of ' +
                 `range. It must be > 0 && < 4294967296. Received ${input}`
      }
    );
  });

  // First argument must be a string
  [0, {}, [], null, Infinity].forEach((input) => {
    assert.throws(
      () => session.altsvc(input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  ['\u0001', 'h2="\uff20"', '👀'].forEach((input) => {
    assert.throws(
      () => session.altsvc(input),
      {
        code: 'ERR_INVALID_CHAR',
        name: 'TypeError',
        message: 'Invalid character in alt'
      }
    );
  });

  [{}, [], true].forEach((input) => {
    assert.throws(
      () => session.altsvc('clear', input),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError'
      }
    );
  });

  [
    'abc:',
    new URL('abc:'),
    { origin: 'null' },
    { origin: '' }
  ].forEach((input) => {
    assert.throws(
      () => session.altsvc('h2=":8000', input),
      {
        code: 'ERR_HTTP2_ALTSVC_INVALID_ORIGIN',
        name: 'TypeError',
        message: 'HTTP/2 ALTSVC frames require a valid origin'
      }
    );
  });

  // Arguments + origin are too long for an ALTSVC frame
  assert.throws(
    () => {
      session.altsvc('h2=":8000"',
                     `http://example.${'a'.repeat(17000)}.org:8000`);
    },
    {
      code: 'ERR_HTTP2_ALTSVC_LENGTH',
      name: 'TypeError',
      message: 'HTTP/2 ALTSVC frames are limited to 16382 bytes'
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
