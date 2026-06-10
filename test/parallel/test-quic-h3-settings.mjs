// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 settings enforcement.
// maxHeaderPairs enforcement - reject headers exceeding pair count
// maxHeaderLength enforcement - reject headers exceeding byte length
// enableConnectProtocol setting (accepted without error)
// enableDatagrams setting (accepted without error)

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:http3');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();

// maxHeaderPairs enforcement.
// Server limits to 5 header pairs. Client sends 4 pseudo-headers +
// 2 custom headers = 6 pairs. The 6th pair is silently dropped.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        strictEqual(headers[':method'], 'GET');
        strictEqual(headers[':path'], '/limited');
        strictEqual(headers[':scheme'], 'https');
        strictEqual(headers[':authority'], 'localhost');
        // x-first is the 5th pair — accepted.
        strictEqual(headers['x-first'], 'one');
        // x-second would be the 6th pair — dropped.
        strictEqual(headers['x-second'], undefined);

        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(encoder.encode('ok'));
        stream.writer.endSync();
      });
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    // Allow 5 header pairs: 4 pseudo-headers + 1 custom.
    application: { maxHeaderPairs: 5 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/limited',
    ':scheme': 'https',
    ':authority': 'localhost',
    'x-first': 'one',
    'x-second': 'two',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await stream.closed;
  await serverDone.promise;
  await clientSession.close();
  await serverEndpoint.close();
}

// maxHeaderLength enforcement.
// Server limits total header byte length (name chars + value chars).
// The 4 pseudo-headers take ~45 bytes. A long custom header value
// pushes the total over the limit.
{
  const serverDone = Promise.withResolvers();
  const longValue = 'x'.repeat(200);

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        strictEqual(headers[':method'], 'GET');
        strictEqual(headers[':path'], '/length-limited');
        // x-long should be dropped — would push total over 100 bytes.
        strictEqual(headers['x-long'], undefined);

        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(encoder.encode('ok'));
        stream.writer.endSync();
      });
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    // Limit total header bytes. The 4 pseudo-headers fit within 100
    // bytes, but adding x-long (6 + 200 = 206 bytes) exceeds it.
    application: { maxHeaderLength: 100 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/length-limited',
    ':scheme': 'https',
    ':authority': 'localhost',
    'x-long': longValue,
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}

// enableConnectProtocol and enableDatagrams settings.
// Verify these options are accepted and H3 sessions work with them.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      stream.onheaders = mustCall((headers) => {
        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(encoder.encode('settings-ok'));
        stream.writer.endSync();
      });
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    application: { enableConnectProtocol: true, enableDatagrams: true },
    onapplication: mustCall((appopt) => {
      strictEqual(appopt.enableDatagrams, true);
      strictEqual(appopt.enableConnectProtocol, false);
      // Must be false, as this is only sent from server side
    })
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    application: { enableConnectProtocol: true, enableDatagrams: true },
  });
  clientSession.onapplication = mustCall((appopt) => {
    strictEqual(appopt.enableConnectProtocol, true);
    strictEqual(appopt.enableDatagrams, true);
  });
  await clientSession.opened;

  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/settings',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    onheaders: mustCall((headers) => {
      strictEqual(headers[':status'], '200');
    }),
  });

  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'settings-ok');
  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}
