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

    // Setting the response.statusCode should not throw.
    response.statusCode = realStatusCodes.ok;
    response.statusCode = realStatusCodes.multipleChoices;
    response.statusCode = realStatusCodes.badRequest;
    response.statusCode = realStatusCodes.internalServerError;

    assert.throws(() => {
      response.statusCode = realStatusCodes.continue;
    }, {
      code: 'ERR_HTTP2_INFO_STATUS_NOT_ALLOWED',
      name: 'RangeError'
    });
    assert.throws(() => {
      response.statusCode = fakeStatusCodes.tooLow;
    }, {
      code: 'ERR_HTTP2_STATUS_INVALID',
      name: 'RangeError'
    });
    assert.throws(() => {
      response.statusCode = fakeStatusCodes.tooHigh;
    }, {
      code: 'ERR_HTTP2_STATUS_INVALID',
      name: 'RangeError'
    });

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
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
