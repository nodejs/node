'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  server.once('request', common.mustCall((request, response) => {
    response.addTrailers({
      ABC: 123
    });
    response.setTrailer('ABCD', 123);

    assert.throws(
      () => response.addTrailers({ '': 'test' }),
      {
        code: 'ERR_INVALID_HTTP_TOKEN',
        name: 'TypeError',
      }
    );
    assert.throws(
      () => response.setTrailer('test', undefined),
      {
        code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
        name: 'TypeError',
      }
    );
    assert.throws(
      () => response.setTrailer('test', null),
      {
        code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
        name: 'TypeError',
      }
    );
    assert.throws(
      () => response.setTrailer(), // Trailer name undefined
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      }
    );
    assert.throws(
      () => response.setTrailer(''),
      {
        code: 'ERR_INVALID_HTTP_TOKEN',
        name: 'TypeError',
      }
    );

    response.end('hello');
  }));

  const url = `http://localhost:${port}`;
  const client = http2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.on('trailers', common.mustCall((headers) => {
      assert.strictEqual(headers.abc, '123');
      assert.strictEqual(headers.abcd, '123');
    }));
    request.resume();
    request.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}));
