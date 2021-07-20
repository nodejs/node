'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', (stream, headers) => {
  assert.deepStrictEqual(headers, {
    ':scheme': 'http',
    ':authority': `localhost:${server.address().port}`,
    ':method': 'GET',
    ':path': '/',
    'bar': '',
    '__proto__': null,
    [http2.sensitiveHeaders]: []
  });
  stream.respond({ ':status': 200 });
  stream.end();
  server.close();
});
server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}/`);
  const req = client.request({ ':path': '/', '': 'foo', 'bar': '' });
  req.on('error', common.mustNotCall());
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => {
    client.close();
  }));
}));
