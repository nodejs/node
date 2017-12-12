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

    common.expectsError(
      () => response.addTrailers({ '': 'test' }),
      {
        code: 'ERR_INVALID_HTTP_TOKEN',
        type: TypeError,
        message: 'Header name must be a valid HTTP token [""]'
      }
    );
    common.expectsError(
      () => response.setTrailer('test', undefined),
      {
        code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
        type: TypeError,
        message: 'Invalid value "undefined" for header "test"'
      }
    );
    common.expectsError(
      () => response.setTrailer('test', null),
      {
        code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
        type: TypeError,
        message: 'Invalid value "null" for header "test"'
      }
    );
    common.expectsError(
      () => response.setTrailer(), // trailer name undefined
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "name" argument must be of type string'
      }
    );
    common.expectsError(
      () => response.setTrailer(''),
      {
        code: 'ERR_INVALID_HTTP_TOKEN',
        type: TypeError,
        message: 'Header name must be a valid HTTP token [""]'
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
