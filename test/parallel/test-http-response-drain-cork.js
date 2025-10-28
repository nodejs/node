'use strict';
const common = require('../common');
const http = require('http');
const assert = require('assert');

// Test that drain event is emitted correctly when using cork/uncork
// with ServerResponse and the write buffer is full

const server = http.createServer(common.mustCall(async (req, res) => {
  res.cork();
  
  // Write small amount - won't need drain
  assert.strictEqual(res.write('1'.repeat(100)), true);
  
  // Write large amount that should require drain
  const needsDrain = !res.write('2'.repeat(1000000));
  
  if (needsDrain) {
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
    
    // Uncork should trigger drain if needed
    res.uncork();
    
    await drainPromise;
    
    // Cork again for next write
    res.cork();
  }
  
  // Write more data
  res.write('3'.repeat(100));
  
  // Final uncork and end
  res.uncork();
  res.end();
}));

server.listen(0, common.mustCall(() => {
  http.get({
    port: server.address().port,
  }, common.mustCall((res) => {
    let data = '';
    res.setEncoding('utf8');
    
    res.on('data', (chunk) => {
      data += chunk;
    });
    
    res.on('end', common.mustCall(() => {
      // Verify we got all the data
      assert.strictEqual(data.length, 100 + 1000000 + 100);
      assert.strictEqual(data.substring(0, 100), '1'.repeat(100));
      assert.strictEqual(data.substring(100, 1000100), '2'.repeat(1000000));
      assert.strictEqual(data.substring(1000100), '3'.repeat(100));
      
      server.close();
    }));
  }));
}));

