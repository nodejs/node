// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerResponse should have a statusCode property

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    const expectedDefaultStatusCode = 200;
    const realStatusCodes = {
      continue: 100,
      ok: 200,
      multipleChoices: 300,
      badRequest: 400,
      internalServerError: 500
    };
    const fakeStatusCodes = {
      tooLow: 99,
      tooHigh: 600
    };

    assert.strictEqual(response.statusCode, expectedDefaultStatusCode);

    assert.doesNotThrow(function() {
      response.statusCode = realStatusCodes.ok;
      response.statusCode = realStatusCodes.multipleChoices;
      response.statusCode = realStatusCodes.badRequest;
      response.statusCode = realStatusCodes.internalServerError;
    });

    assert.throws(function() {
      response.statusCode = realStatusCodes.continue;
    }, common.expectsError({
      code: 'ERR_HTTP2_INFO_STATUS_NOT_ALLOWED',
      type: RangeError
    }));
    assert.throws(function() {
      response.statusCode = fakeStatusCodes.tooLow;
    }, common.expectsError({
      code: 'ERR_HTTP2_STATUS_INVALID',
      type: RangeError
    }));
    assert.throws(function() {
      response.statusCode = fakeStatusCodes.tooHigh;
    }, common.expectsError({
      code: 'ERR_HTTP2_STATUS_INVALID',
      type: RangeError
    }));

    response.on('finish', common.mustCall(function() {
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
