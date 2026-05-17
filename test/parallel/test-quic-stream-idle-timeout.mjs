// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream idle timeout.
// Peer-initiated streams that receive no data within the configured
// timeout are automatically destroyed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';
import { text } from 'node:stream/iter';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// --- Stream destroyed after idle timeout ---
{
  const streamDestroyed = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      // Don't read — let the stream sit idle after the initial data.
      // The stream idle timeout should destroy it, rejecting stream.closed.
      await rejects(stream.closed, {
        code: 'ERR_QUIC_TRANSPORT_ERROR',
      });
      streamDestroyed.resolve();
    });
  }), {
    // Short idle timeout for the test — 500ms.
    streamIdleTimeout: 100,
  });

  const clientSession = await connect(serverEndpoint.address, {
    verifyPeer: 'manual',
    transportParams: { maxIdleTimeout: 1 },
  });

  await clientSession.opened;

  // Open a stream and send one byte so the server creates the
  // peer-initiated stream, then stop sending.
  const clientStream = await clientSession.createUnidirectionalStream();
  const writer = clientStream.writer;
  writer.writeSync('x');

  // Wait for the server to destroy the idle stream.
  await streamDestroyed.promise;

  // The server sent STOP_SENDING when it destroyed the idle stream.
  // ShutdownStream maps the transport error to the application's
  // internal error code on the wire, so the client sees an application
  // error indicating the server rejected the stream.
  await rejects(clientStream.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
  });

  await clientSession.close();
  await serverEndpoint.close();
}

// --- Stream with data is NOT destroyed ---
{
  const encoder = new TextEncoder();
  const serverGotData = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const data = await text(stream);
      strictEqual(data, 'xy');
      serverGotData.resolve();
      await serverSession.close();
    });
  }), {
    streamIdleTimeout: 500,
  });

  const clientSession = await connect(serverEndpoint.address, {
    verifyPeer: 'manual',
  });
  await clientSession.opened;

  // Open a stream and send data immediately.
  const clientStrema = await clientSession.createUnidirectionalStream();
  const writer = clientStrema.writer;
  writer.writeSync(encoder.encode('x'));

  // Less than the 500ms idle timeout.
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

      // Do not await this yet.
      const data = await text(stream);

      // We should receive all the data, even though it is sent with a pause
      // longer than the default idle timeout.
      strictEqual(data, 'xy');
      streamSurvived.resolve();
    });

    await setTimeout(700);
    await serverSession.close();
  }), {
    streamIdleTimeout: 0,  // Disabled
  });

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Send one byte so the server creates the stream, then stop.
  const clientStream = await clientSession.createUnidirectionalStream();
  const writer = clientStream.writer;
  writer.writeSync('x');

  // Now pause beyond the 500ms default timeout to verify the stream is not
  // killed by idle timeout.
  await setTimeout(600);

  writer.writeSync('y');
  writer.endSync();

  await Promise.all([streamSurvived.promise, clientSession.closed]);
  await serverEndpoint.close();
}
