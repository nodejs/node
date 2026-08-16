// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 webtransport client initiated uni stream
// Client creates an uni stream and send data to the server

import { hasQuic, skip, mustCall, mustNotCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { drainableProtocol: dp } = await import('stream/iter');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const chunkSizes = [60000, 12, 1000000, 50000, 1600, 20000, 1000000, 30000, 0, 100];
const numChunks = chunkSizes.length;
const byteLength = chunkSizes.reduce((accumulator, currentValue) => accumulator + currentValue, 0);


// Build a deterministic payload so we can verify integrity.
function buildChunk(index) {
  const chunk = new Uint8Array(chunkSizes[index]);
  // Fill with a pattern derived from the chunk index.
  const val = index & 0xff;
  for (let i = 0; i < chunkSizes[index]; i++) {
    chunk[i] = (val + i) & 0xff;
  }
  return chunk;
}

function checksum(data) {
  let sum = 0;
  for (let i = 0; i < data.byteLength; i++) {
    sum = (sum + data[i]) | 0;
  }
  return sum;
}

// Compute expected checksum.
let expectedChecksum = 0;
for (let i = 0; i < numChunks; i++) {
  const chunk = buildChunk(i);
  expectedChecksum = (expectedChecksum + checksum(chunk)) | 0;
}

const serverSessionOpened = Promise.withResolvers();

const readChunks = [];
let serverSessionStream;
const serverEndpoint = await listen(mustCall(async (ss) => {
  ss.onapplication = mustCall((aopts) => {
    assert.strictEqual(!aopts.enableDatagrams, false);
  });
  ss.onhandshake = mustCall(function() {
    assert.strictEqual(BigInt(this?.remoteTransportParams?.maxDatagramFrameSize) > 0, true);
  });
  ss.onstream = mustCallAtLeast((stream) => {
    stream.onsessionid = mustCallAtLeast(async (sessionid) => {
      // Deinstall handlers
      stream.onheaders = undefined;
      stream.onsessionid = undefined;
      const sessionid2 = await serverSessionOpened.promise;
      assert.strictEqual(sessionid, sessionid2);
      // Now we get the data
      for await (const chunks of stream) {
        readChunks.push(...chunks);
      }
      const receivedBytes = readChunks.reduce((accu, curVal) => accu + curVal.byteLength, 0);

      assert.strictEqual(receivedBytes, byteLength);
      let receivedChecksum = 0;
      for (const chunk of readChunks) {
        receivedChecksum = (receivedChecksum + checksum(chunk)) | 0;
      }
      assert.strictEqual(receivedChecksum, expectedChecksum);
      serverSessionStream.closed.catch(mustCall((error) => {
        assert.strictEqual(error.errorCode, 200n);
        assert.strictEqual(error.reason, 'all perfect');
      }));
      await serverSessionStream.closeWebtransportSessionStream(200, 'all perfect');
    }, stream.id !== 0n ? 1 : 0); // The second stream is a datastream
  }, 2);
}), {
  sni: { '*': { keys: [key], certs: [cert] } },
  application: {
    enableConnectProtocol: true,
    enableDatagrams: true,
    enableWebtransport: true
  },
  transportParams: {
    maxDatagramFrameSize: 100,
    initialMaxStreamsBidi: 100, // Default value according to spec
    initialMaxStreamsUni: 100, // Especially important as limit default is 0
  },
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
      serverSessionOpened.resolve(this.id);
      serverSessionStream = this;
    } catch (error) {
      serverSessionOpened.reject(error);
    }
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
  transportParams: {
    maxDatagramFrameSize: 1000,
    initialMaxStreamsBidi: 100, // Default value according to spec
    initialMaxStreamsUni: 100, // Especially important as limit default is 0
  },
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
    console.log('Called onheaders on the client side!');
  });
});

await clientSession.opened;
await webtransportSupport.promise;
// Now we open a webtransport session, which is actually
// a special bidirectional stream
const wtSessionStream = await clientSession.createBidirectionalStream({
  body: '',
});
wtSessionStream.onwtsessionclose = mustCall((code, reason) => {
  assert.strictEqual(code, 200);
  assert.strictEqual(reason, 'all perfect');
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

// Well let's get a unidi stream and send something
const clientUnidiStream = await clientSession.createUnidirectionalStream({
  incremental: true,
  webtransportSession: wtSessionStream // Associate it with the sessionStream
});

// Next step send some data
await serverSessionOpened.promise;

const w = clientUnidiStream.writer;
for (let i = 0; i < numChunks; i++) {
  const chunk = buildChunk(i);
  while (!w.writeSync(chunk)) {
    // Flow controlled — wait for drain before retrying.
    const drainable = w[dp]();
    if (drainable) await drainable;
  }
}
w.endSync();

try {
  await wtSessionStream.closed;
} catch (error) {
  assert.strictEqual(error.errorCode, 200n);
  assert.strictEqual(error.reason, 'all perfect');
}

await clientSession.close();
await serverEndpoint.close();
