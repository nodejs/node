'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }

// Test for Http2ServerResponse#[writableCorked,cork,uncork]

const assert = require('assert');
const http2 = require('http2');

{
  let corksLeft = 0;
  const server = http2.createServer(common.mustCall((req, res) => {
    assert.strictEqual(res.writableCorked, corksLeft);
    res.write(Buffer.from('1'.repeat(1024)));
    res.cork();
    corksLeft++;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.write(Buffer.from('1'.repeat(1024)));
    res.cork();
    corksLeft++;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.write(Buffer.from('1'.repeat(1024)));
    res.cork();
    corksLeft++;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.write(Buffer.from('1'.repeat(1024)));
    res.cork();
    corksLeft++;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.uncork();
    corksLeft--;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.uncork();
    corksLeft--;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.uncork();
    corksLeft--;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.uncork();
    corksLeft--;
    assert.strictEqual(res.writableCorked, corksLeft);
    res.end();
  }));
  server.listen(0, common.mustCall(() => {
    const URL = `http://localhost:${server.address().port}`;
    const client = http2.connect(URL);
    const req = client.request();
    req.on('data', common.mustCall(2));
    req.on('end', common.mustCall(() => {
      server.close();
      client.close();
    }));
  }));
}
