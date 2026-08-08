// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 webtransport session establishment
// Verifies that a session established and properly closed

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { setTimeout: sleep } = await import('timers/promises');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const serverSessionOpened = Promise.withResolvers();


const serverEndpoint = await listen(mustCall(async (ss) => {
  ss.onstream = mustCall(async (stream) => {
    console.log('Does nothing but is required');
  });
  ss.onapplication = mustCall((aopts) => {
    assert.strictEqual(!aopts.enableDatagrams, false);
  });
  ss.onhandshake = mustCall(function() {
    assert.strictEqual(BigInt(this?.remoteTransportParams?.maxDatagramFrameSize) > 0, true);
  });
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  application: {
    enableConnectProtocol: true,
    enableDatagrams: true,
    enableWebtransport: true
  },
  transportParams: { maxDatagramFrameSize: 100 },
  onheaders: mustCall(function(headers) {
    try {
      assert.strictEqual(headers[':scheme'], 'https');
      assert.strictEqual(headers[':method'], 'CONNECT');
      assert.strictEqual(headers[':protocol'], 'webtransport'); // depends on the draft
      assert.strictEqual(headers[':path'], '/testwtpath');
      // We could also check for wt-available protocols
      this.sendHeaders(
        { ':status': '200' },
        { terminal: false, webtransport: true }
      );
      serverSessionOpened.resolve();
    } catch (error) {
      serverSessionOpened.reject(error);
    }
    // Should only be installed on wt streams
    this.onwtsessionclose = mustCall((code, reason) => {
      assert.strictEqual(code, 200);
      assert.strictEqual(reason, 'all perfect');
      this.session.close();
    });
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  servername: 'localhost',
  verifyPeer: 'manual',
  application: {
    enableConnectProtocol: true,
    enableDatagrams: true,
    enableWebtransport: true
  },
  transportParams: { maxDatagramFrameSize: 1000 },
});

const webtransportSupport = Promise.withResolvers();

clientSession.onapplication = mustCall((aopts) => {
  try {
    // Test for webtransport support
    assert.strictEqual(aopts.enableConnectProtocol, true);
    assert.strictEqual(aopts.enableDatagrams, true);
    assert.strictEqual(aopts.enableWebtransport, true);
    // Ok we have wt support
    webtransportSupport.resolve();
  } catch (error) {
    webtransportSupport.reject(error);
  }
});

clientSession.onstream = mustNotCall((stream) => {
  stream.onheaders = mustNotCall((stream) => {
    // Well this should not happen on client side
    console.log('Called forbidden onheaders!');
  });
});

await clientSession.opened;
await webtransportSupport.promise;
// Now we open a webtransport session, which is actually
// a special bidirectional stream
const wtSessionStream = await clientSession.createBidirectionalStream({
  body: '',
});
wtSessionStream.sendHeaders({
  ':method': 'CONNECT',
  ':scheme': 'https',
  // This one depends on draft, draft14 says "webtransport", draft15 says "webtransport-h3"
  ':protocol': 'webtransport',
  ':path': '/testwtpath',
  ':authority': 'testserver:' + serverEndpoint.address.port
}, {
  webtransport: true // Tell nghttp3 to treat the stream as a WT session stream
});
await serverSessionOpened.promise;
await sleep(500);
await wtSessionStream.closeWebtransportSessionStream(200, 'all perfect');
try {
  await wtSessionStream.closed;
} catch (error) {
  assert.strictEqual(error.errorCode, 200n);
  assert.strictEqual(error.reason, 'all perfect');
}

await clientSession.close();
await serverEndpoint.close();
