'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

{
  const server = http2.createServer();
  server.on('stream', common.mustCall((stream, _headers, _flags, rawHeaders) => {
    assert.deepStrictEqual(rawHeaders, [
      ':method', 'GET',
      ':authority', `localhost:${server.address().port}`,
      ':scheme', 'http',
      ':path', '/',
      'a', 'b',
      'x-foo', 'bar', // Lowercased as required for HTTP/2
      'a', 'c', // Duplicate header order preserved
    ]);
    stream.respond([
      'x', '1',
      'x-FOO', 'bar',
      'x', '2',
    ]);
    stream.end();
  }));


  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http2.connect(`http://localhost:${port}`);

    const req = client.request([
      'a', 'b',
      'x-FOO', 'bar',
      'a', 'c',
    ]).end();

    assert.deepStrictEqual(req.sentHeaders, {
      '__proto__': null,
      ':path': '/',
      ':scheme': 'http',
      ':authority': `localhost:${server.address().port}`,
      ':method': 'GET',
      'a': [ 'b', 'c' ],
      'x-FOO': 'bar',
    });

    req.on('response', common.mustCall((_headers, _flags, rawHeaders) => {
      assert.strictEqual(rawHeaders.length, 10);
      assert.deepStrictEqual(rawHeaders.slice(0, 8), [
        ':status', '200',
        'x', '1',
        'x-foo', 'bar', // Lowercased as required for HTTP/2
        'x', '2', // Duplicate header order preserved
      ]);

      assert.strictEqual(rawHeaders[8], 'date');
      assert.strictEqual(typeof rawHeaders[9], 'string');

      client.close();
      server.close();
    }));
  }));
}
