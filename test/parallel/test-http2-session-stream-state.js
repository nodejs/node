// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {

  // Test Stream State.
  {
    const state = stream.state;
    assert.strictEqual(typeof state, 'object');
    assert.strictEqual(typeof state.state, 'number');
    assert.strictEqual(typeof state.weight, 'number');
    assert.strictEqual(typeof state.sumDependencyWeight, 'number');
    assert.strictEqual(typeof state.localClose, 'number');
    assert.strictEqual(typeof state.remoteClose, 'number');
    assert.strictEqual(typeof state.localWindowSize, 'number');
  }

  // Test Session State.
  {
    const state = stream.session.state;
    assert.strictEqual(typeof state, 'object');
    assert.strictEqual(typeof state.effectiveLocalWindowSize, 'number');
    assert.strictEqual(typeof state.effectiveRecvDataLength, 'number');
    assert.strictEqual(typeof state.nextStreamID, 'number');
    assert.strictEqual(typeof state.localWindowSize, 'number');
    assert.strictEqual(typeof state.lastProcStreamID, 'number');
    assert.strictEqual(typeof state.remoteWindowSize, 'number');
    assert.strictEqual(typeof state.outboundQueueSize, 'number');
    assert.strictEqual(typeof state.deflateDynamicTableSize, 'number');
    assert.strictEqual(typeof state.inflateDynamicTableSize, 'number');
  }

  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('hello world');
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const headers = { ':path': '/' };

  const req = client.request(headers);

  // State will only be valid after connect event is emitted
  req.on('connect', common.mustCall(() => {

    // Test Stream State.
    {
      const state = req.state;
      assert.strictEqual(typeof state, 'object');
      assert.strictEqual(typeof state.state, 'number');
      assert.strictEqual(typeof state.weight, 'number');
      assert.strictEqual(typeof state.sumDependencyWeight, 'number');
      assert.strictEqual(typeof state.localClose, 'number');
      assert.strictEqual(typeof state.remoteClose, 'number');
      assert.strictEqual(typeof state.localWindowSize, 'number');
    }

    // Test Session State
    {
      const state = req.session.state;
      assert.strictEqual(typeof state, 'object');
      assert.strictEqual(typeof state.effectiveLocalWindowSize, 'number');
      assert.strictEqual(typeof state.effectiveRecvDataLength, 'number');
      assert.strictEqual(typeof state.nextStreamID, 'number');
      assert.strictEqual(typeof state.localWindowSize, 'number');
      assert.strictEqual(typeof state.lastProcStreamID, 'number');
      assert.strictEqual(typeof state.remoteWindowSize, 'number');
      assert.strictEqual(typeof state.outboundQueueSize, 'number');
      assert.strictEqual(typeof state.deflateDynamicTableSize, 'number');
      assert.strictEqual(typeof state.inflateDynamicTableSize, 'number');
    }
  }));

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();

}));
