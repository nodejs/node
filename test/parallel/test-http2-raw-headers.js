'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

{
  const server = http2.createServer();
  server.on('stream', common.mustCall((stream, headers, flags, rawHeaders) => {
    assert.deepStrictEqual(rawHeaders, [
      ':path', '/foobar',
      ':scheme', 'http',
      ':authority', `localhost:${server.address().port}`,
      ':method', 'GET',
      'a', 'b',
      'x-foo', 'bar',
      'a', 'c',
    ]);
    stream.respond({
      ':status': 200
    });
    stream.end();
  }));


  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http2.connect(`http://localhost:${port}`);

    const req = client.request([
      ':path', '/foobar',
      ':scheme', 'http',
      ':authority', `localhost:${server.address().port}`,
      ':method', 'GET',
      'a', 'b',
      'x-FOO', 'bar',
      'a', 'c',
    ]).end();

    assert.deepStrictEqual(req.sentHeaders, {
      '__proto__': null,
      ':path': '/foobar',
      ':scheme': 'http',
      ':authority': `localhost:${server.address().port}`,
      ':method': 'GET',
      'a': [ 'b', 'c' ],
      'x-FOO': 'bar',
    });

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      client.close();
      server.close();
    }));
  }));
}
