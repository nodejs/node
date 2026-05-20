'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');

{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    res.writeInformation(110, { 'X-Progress': '50%' });
    res.writeInformation(199, { 'X-Custom': 'one' });
    res.end('done');
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    const seen = [];
    req.on('headers', (headers) => {
      seen.push({
        status: headers[':status'],
        headers,
      });
    });

    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 200);
      assert.strictEqual(seen.length, 2);
      assert.strictEqual(seen[0].status, 110);
      assert.strictEqual(seen[0].headers['x-progress'], '50%');
      assert.strictEqual(seen[1].status, 199);
      assert.strictEqual(seen[1].headers['x-custom'], 'one');
    }));

    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}

// Error cases.
{
  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    assert.throws(() => res.writeInformation(101),
                  { code: 'ERR_HTTP2_STATUS_INVALID' });
    assert.throws(() => res.writeInformation(200),
                  { code: 'ERR_HTTP2_STATUS_INVALID' });
    assert.throws(() => res.writeInformation('100'),
                  { code: 'ERR_HTTP2_STATUS_INVALID' });
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();
    req.resume();
    req.on('end', common.mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}
