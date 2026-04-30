// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 POST request with FileHandle body.
// Client sends a POST with an fd-backed body source. Server reads the body
// and echoes it back in the response. Verifies that the FdEntry async I/O
// path works correctly through the H3 application layer.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
import { writeFileSync } from 'node:fs';
import { open } from 'node:fs/promises';

const tmpdir = await import('../common/tmpdir.js');

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const decoder = new TextDecoder();
const testContent = 'Hello from a file!\nLine two.\n';

tmpdir.refresh();
const testFile = tmpdir.resolve('quic-h3-fh-test.txt');
writeFileSync(testFile, testContent);

// FileHandle as POST body in createBidirectionalStream.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), testContent);

      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':method'], 'POST');
      strictEqual(headers[':path'], '/upload');

      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync('ok');
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });

  const info = await clientSession.opened;
  strictEqual(info.protocol, 'h3');

  const clientHeadersReceived = Promise.withResolvers();

  const fh = await open(testFile, 'r');
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'POST',
      ':path': '/upload',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    body: fh,
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
      clientHeadersReceived.resolve();
    }),
  });

  await clientHeadersReceived.promise;

  const responseBody = await bytes(stream);
  strictEqual(decoder.decode(responseBody), 'ok');

  await Promise.all([stream.closed, serverDone.promise]);
  clientSession.close();
  await clientSession.closed;
  await serverEndpoint.close();
  // FileHandle is closed automatically when the stream finishes.
}
