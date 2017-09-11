// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse should support checking and reading custom headers

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    const real = 'foo-bar';
    const fake = 'bar-foo';
    const denormalised = ` ${real.toUpperCase()}\n\t`;
    const expectedValue = 'abc123';

    response.setHeader(real, expectedValue);

    assert.strictEqual(response.hasHeader(real), true);
    assert.strictEqual(response.hasHeader(fake), false);
    assert.strictEqual(response.hasHeader(denormalised), true);
    assert.strictEqual(response.getHeader(real), expectedValue);
    assert.strictEqual(response.getHeader(denormalised), expectedValue);
    assert.strictEqual(response.getHeader(fake), undefined);

    response.removeHeader(fake);
    assert.strictEqual(response.hasHeader(fake), false);

    response.setHeader(real, expectedValue);
    assert.strictEqual(response.getHeader(real), expectedValue);
    assert.strictEqual(response.hasHeader(real), true);
    response.removeHeader(real);
    assert.strictEqual(response.hasHeader(real), false);

    response.setHeader(denormalised, expectedValue);
    assert.strictEqual(response.getHeader(denormalised), expectedValue);
    assert.strictEqual(response.hasHeader(denormalised), true);
    response.removeHeader(denormalised);
    assert.strictEqual(response.hasHeader(denormalised), false);

    [
      ':status',
      ':method',
      ':path',
      ':authority',
      ':scheme'
    ].forEach((header) => assert.throws(
      () => response.setHeader(header, 'foobar'),
      common.expectsError({
        code: 'ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED',
        type: Error,
        message: 'Cannot set HTTP/2 pseudo-headers'
      })
    ));
    assert.throws(function() {
      response.setHeader(real, null);
    }, common.expectsError({
      code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
      type: TypeError,
      message: 'Value must not be undefined or null'
    }));
    assert.throws(function() {
      response.setHeader(real, undefined);
    }, common.expectsError({
      code: 'ERR_HTTP2_INVALID_HEADER_VALUE',
      type: TypeError,
      message: 'Value must not be undefined or null'
    }));

    response.setHeader(real, expectedValue);
    const expectedHeaderNames = [real];
    assert.deepStrictEqual(response.getHeaderNames(), expectedHeaderNames);
    const expectedHeaders = { [real]: expectedValue };
    assert.deepStrictEqual(response.getHeaders(), expectedHeaders);

    response.getHeaders()[fake] = fake;
    assert.strictEqual(response.hasHeader(fake), false);

    assert.strictEqual(response.sendDate, true);
    response.sendDate = false;
    assert.strictEqual(response.sendDate, false);

    assert.strictEqual(response.code, h2.constants.NGHTTP2_NO_ERROR);

    response.on('finish', common.mustCall(function() {
      assert.strictEqual(response.code, h2.constants.NGHTTP2_NO_ERROR);
      assert.strictEqual(response.headersSent, true);
      server.close();
    }));
    response.end();
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('end', common.mustCall(function() {
      client.destroy();
    }));
    request.end();
    request.resume();
  }));
}));
