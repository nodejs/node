'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');

// writeProcessing() sends a 102 Processing informational response, with or
// without a callback, and the callback is called asynchronously once it has
// been written to the socket.
{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    let sync = true;

    assert.strictEqual(res.writeProcessing(), true);
    assert.strictEqual(res.writeProcessing(common.mustCall((err) => {
      assert.strictEqual(err, null);
      assert.strictEqual(sync, false);
      res.end('done');
    })), true);
    sync = false;
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    const seen = [];
    req.on('headers', (headers) => seen.push(headers[':status']));
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      assert.deepStrictEqual(seen, [102, 102]);
    }));

    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// A non-function callback is rejected, and nothing is sent once the final
// response headers have been sent.
{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    for (const cb of [null, 1, 'cb', {}]) {
      assert.throws(() => res.writeProcessing(cb),
                    { code: 'ERR_INVALID_ARG_TYPE' });
    }

    res.writeHead(200);
    assert.strictEqual(res.writeProcessing(common.mustNotCall()), false);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.on('headers', common.mustNotCall());
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
    }));
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}
