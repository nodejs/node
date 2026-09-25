'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

// Test that drain event is emitted correctly when using cork/uncork
// with ServerResponse and the write buffer is full

const server = http.createServer(common.mustCall(async (req, res) => {
  res.cork();

  // Write small amount - won't need drain
  assert.strictEqual(res.write('1'.repeat(10)), true);

  // Write enough to exceed highWaterMark (set in 'connection' listener)
  assert.strictEqual(res.write('2'.repeat(1000)), false);

  // Verify writableNeedDrain is set
  assert.strictEqual(res.writableNeedDrain, true);

  // Wait for drain event after uncorking
  const drainPromise = new Promise((resolve) => {
    res.once('drain', common.mustCall(() => {
      // After drain, writableNeedDrain should be false
      assert.strictEqual(res.writableNeedDrain, false);
      resolve();
    }));
  });

  // Uncork should trigger drain
  res.uncork();
  await drainPromise;

  res.end();
}));

server.on('connection', common.mustCall((socket) => {
  // Set high water mark large enough to cover HTTP overhead + first
  // small content batch, but not enough to cover second batch.
  socket._writableState.highWaterMark = 1000;
}));

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  http.get({
    host: common.localhostIPv4,
    port: server.address().port,
  }, common.mustCall((res) => {
    let data = '';
    res.setEncoding('utf8');

    res.on('data', (chunk) => {
      data += chunk;
    });

    res.on('end', common.mustCall(() => {
      // Verify we got all the data
      assert.strictEqual(data.length, 10 + 1000);
      assert.strictEqual(data.substring(0, 10), '1'.repeat(10));
      assert.strictEqual(data.substring(10), '2'.repeat(1000));

      server.close(common.mustCall());
    }));
  }));
}));
