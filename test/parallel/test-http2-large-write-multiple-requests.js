'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');

const content = fixtures.readSync('person-large.jpg');

const server = http2.createServer({
  maxSessionMemory: 1000
});
server.on('stream', (stream, headers) => {
  stream.respond({
    'content-type': 'image/jpeg',
    ':status': 200
  });
  stream.end(content);
});
server.unref();

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}/`);

  let finished = 0;
  for (let i = 0; i < 100; i++) {
    const req = client.request({ ':path': '/' }).end();
    const chunks = [];
    req.on('data', (chunk) => {
      chunks.push(chunk);
    });
    req.on('end', common.mustCall(() => {
      assert.deepStrictEqual(Buffer.concat(chunks), content);
      if (++finished === 100) client.close();
    }));
  }
}));
