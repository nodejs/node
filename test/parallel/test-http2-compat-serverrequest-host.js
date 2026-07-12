'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Requests using host instead of :authority should be allowed
// and Http2ServerRequest.authority should fall back to host

// :authority should NOT be auto-filled if host is present

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    const expected = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      'host': `localhost:${port}`
    };

    assert.strictEqual(request.authority, expected.host);

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

    assert(!Object.hasOwn(headers, ':authority'));
    assert(!Object.hasOwn(rawHeaders, ':authority'));

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
      'host': `localhost:${port}`
    };
    const request = client.request(headers);
    request.on('end', common.mustCall(function() {
      client.close();
    }));
    request.end();
    request.resume();
  }));
}));
