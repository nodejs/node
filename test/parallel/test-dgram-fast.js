// Flags: --expose-internals --no-warnings --allow-natives-syntax --test-udp-no-try-send
'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const { internalBinding } = require('internal/test/binding');

function testFastDgram() {
  const server = dgram.createSocket('udp4');
  const client = dgram.createSocket('udp4');

  server.bind(0, common.mustCall(() => {
    client.connect(server.address().port, common.mustCall(() => {
      client.send('Hello');
      client.send('World');

      assert.strictEqual(typeof client.getSendQueueSize(), 'number');
      assert.strictEqual(typeof client.getSendQueueCount(), 'number');

      client.close();
      server.close();
    }));
  }));
}

eval('%PrepareFunctionForOptimization(testFastDgram)');
testFastDgram();
eval('%OptimizeFunctionOnNextCall(testFastDgram)');
testFastDgram();

if (common.isDebug) {
  const { getV8FastApiCallCount } = internalBinding('debug');
  assert.strictEqual(getV8FastApiCallCount('udp.getSendQueueSize'), 0);
  assert.strictEqual(getV8FastApiCallCount('udp.getSendQueueCount'), 0);
}
