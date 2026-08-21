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
      ':path', '/foobar',
      ':scheme', 'http',
      ':authority', `test.invalid:${server.address().port}`,
      ':method', 'POST',
      'a', 'b',
      'x-foo', 'bar', // Lowercased as required for HTTP/2
      'a', 'c', // Duplicate header order preserved
    ]);

    stream.respond([
      ':status', '404',
      'x', '1',
      'x-FOO', 'bar',
      'x', '2',
      'DATE', '0000',
    ]);

    assert.deepStrictEqual(stream.sentHeaders, {
      '__proto__': null,
      ':status': '404',
      'x': [ '1', '2' ],
      'x-FOO': 'bar',
      'DATE': '0000',
    });

    stream.end();
  }));


  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http2.connect(`http://localhost:${port}`);

    const req = client.request([
      ':path', '/foobar',
      ':scheme', 'http',
      ':authority', `test.invalid:${server.address().port}`,
      ':method', 'POST',
      'a', 'b',
      'x-FOO', 'bar',
      'a', 'c',
    ]).end();

    assert.deepStrictEqual(req.sentHeaders, {
      '__proto__': null,
      ':path': '/foobar',
      ':scheme': 'http',
      ':authority': `test.invalid:${server.address().port}`,
      ':method': 'POST',
      'a': [ 'b', 'c' ],
      'x-FOO': 'bar',
    });

    req.on('response', common.mustCall((_headers, _flags, rawHeaders) => {
      assert.deepStrictEqual(rawHeaders, [
        ':status', '404',
        'x', '1',
        'x-foo', 'bar', // Lowercased as required for HTTP/2
        'x', '2', // Duplicate header order preserved
        'date', '0000', // Server doesn't automatically set its own value
      ]);
      client.close();
      server.close();
    }));
  }));
}
