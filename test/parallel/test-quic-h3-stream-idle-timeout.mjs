// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 stream idle timeout.
// Peer-initiated streams that receive no data within the configured
// timeout are automatically destroyed. This test verifies the behavior
// with HTTP/3 bidirectional streams, where ShutdownStream maps
// transport errors to the application's internal error code
// (NGHTTP3_H3_INTERNAL_ERROR = 0x102) on the wire.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';
import * as fixtures from '../common/fixtures.mjs';
import { text } from 'node:stream/iter';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

const encoder = new TextEncoder();

// --- H3 stream destroyed after idle timeout ---
{
  const streamDestroyed = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      // Don't read — let the stream sit idle after the initial headers.
      // The stream idle timeout should destroy it, rejecting stream.closed.
      await assert.rejects(stream.closed, {
        code: 'ERR_QUIC_TRANSPORT_ERROR',
      });
      streamDestroyed.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    streamIdleTimeout: 100,
    onheaders() {
      // Receive headers but do nothing — let the stream go idle.
    },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });

  await clientSession.opened;

  // Send a POST request with a body byte so the server creates the
  // stream, then stop sending (don't end the write side).
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'POST',
      ':path': '/',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders() {},
  });
  const writer = stream.writer;
  writer.writeSync(encoder.encode('x'));

  // Wait for the server to destroy the idle stream.
  await streamDestroyed.promise;

  // The server sent STOP_SENDING / RESET_STREAM when it destroyed the
  // idle stream. ShutdownStream maps the transport error to the H3
  // internal error code (0x102) on the wire.
  await assert.rejects(stream.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
  });

  await clientSession.close();
  await serverEndpoint.close();
}

// --- H3 stream with ongoing data is NOT destroyed ---
{
  const serverGotData = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const data = await text(stream);
      assert.strictEqual(data, 'xy');
      serverGotData.resolve();
      await serverSession.close();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    streamIdleTimeout: 500,
    onheaders: mustCall(function(headers) {
      assert.strictEqual(headers[':method'], 'POST');
      // Send response headers so the stream is fully established.
      this.sendHeaders({ ':status': '200' }, { terminal: true });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    onheaders() {},
  });
  stream.sendHeaders({
    ':method': 'POST',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
    'content-length': '2',
  }, { terminal: false });
  const writer = stream.writer;
  writer.writeSync(encoder.encode('x'));

  // Wait less than the 500ms idle timeout, then send more data.
  await setTimeout(300);

  writer.writeSync(encoder.encode('y'));
  writer.endSync();

  await Promise.all([serverGotData.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// --- Disabled when set to 0 ---
{
  const streamSurvived = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const data = await text(stream);
      assert.strictEqual(data, 'xy');
      streamSurvived.resolve();
    });

    await setTimeout(700);
    await serverSession.close();
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    streamIdleTimeout: 0,  // Disabled
    onheaders: mustCall(function(headers) {
      this.sendHeaders({ ':status': '200' }, { terminal: true });
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    onheaders() {},
  });
  stream.sendHeaders({
    ':method': 'POST',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, { terminal: false });
  const writer = stream.writer;
  writer.writeSync(encoder.encode('x'));

  // Pause beyond any reasonable idle timeout to verify it's disabled.
  await setTimeout(600);

  writer.writeSync(encoder.encode('y'));
  writer.endSync();

  await Promise.all([streamSurvived.promise, clientSession.closed]);
  await serverEndpoint.close();
}
