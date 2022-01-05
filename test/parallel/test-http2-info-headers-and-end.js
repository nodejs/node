'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');

const server = http2.createServer((request, response) => {
  response.writeContinue();
  response.end();
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  {
    const req = client.request({ ':path': '/' });

    req.on('headers', common.mustCall((headers) => {
      assert.notStrictEqual(headers, undefined);
      assert.strictEqual(headers[':status'], 100);
    }));

    req.on('response', common.mustCall((headers) => {
      assert.notStrictEqual(headers, undefined);
      assert.strictEqual(headers[':status'], 200);
    }));

    req.on('trailers', common.mustNotCall());

    req.resume();
    req.on('end', () => {
      client.close();
      server.close();
    });
    req.end();
  }
}));
