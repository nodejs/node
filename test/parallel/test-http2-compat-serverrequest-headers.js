'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerRequest should have header helpers

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    const expected = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
      'foo-bar': 'abc123'
    };

    assert.strictEqual(request.path, undefined);
    assert.strictEqual(request.method, expected[':method']);
    assert.strictEqual(request.scheme, expected[':scheme']);
    assert.strictEqual(request.url, expected[':path']);
    assert.strictEqual(request.authority, expected[':authority']);

    const headers = request.headers;
    for (const [name, value] of Object.entries(expected)) {
      assert.strictEqual(headers[name], value);
    }

    const rawHeaders = request.rawHeaders;
    for (const [name, value] of Object.entries(expected)) {
      const position = rawHeaders.indexOf(name);
      assert.notStrictEqual(position, -1);
      assert.strictEqual(rawHeaders[position + 1], value);
    }

    request.url = '/one';
    assert.strictEqual(request.url, '/one');

    // Third-party plugins for packages like express use query params to
    // change the request method
    request.method = 'POST';
    assert.strictEqual(request.method, 'POST');
    assert.throws(
      () => request.method = '   ',
      {
        code: 'ERR_INVALID_ARG_VALUE',
        name: 'TypeError [ERR_INVALID_ARG_VALUE]',
        message: "The argument 'method' is invalid. Received '   '"
      }
    );
    assert.throws(
      () => request.method = true,
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError [ERR_INVALID_ARG_TYPE]',
        message: 'The "method" argument must be of type string. ' +
                 'Received type boolean'
      }
    );

    response.on('finish', common.mustCall(function() {
      server.close();
    }));
    response.end();
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
      'foo-bar': 'abc123'
    };
    const request = client.request(headers);
    request.on('end', common.mustCall(function() {
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
