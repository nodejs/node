// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: FileHandle as body source for QUIC streams.
// The file contents are sent via an fd-backed DataQueue. The FileHandle
// is automatically closed when the stream finishes.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { writeFileSync } from 'node:fs';
import { open } from 'node:fs/promises';

const tmpdir = await import('../common/tmpdir.js');

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const decoder = new TextDecoder();
const testContent = 'Hello from a file!\nLine two.\n';

tmpdir.refresh();
const testFile = tmpdir.resolve('quic-fh-test.txt');
writeFileSync(testFile, testContent);

// FileHandle as body in createBidirectionalStream.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), testContent);
      stream.writer.writeSync('ok');
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const fh = await open(testFile, 'r');
  const stream = await clientSession.createBidirectionalStream({
    body: fh,
  });

  const response = await bytes(stream);
  strictEqual(decoder.decode(response), 'ok');
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
  // FileHandle is closed automatically when the stream finishes.
}

// FileHandle as body in setBody.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), testContent);
      stream.writer.writeSync('ok');
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const fh = await open(testFile, 'r');
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(fh);

  const response = await bytes(stream);
  strictEqual(decoder.decode(response), 'ok');
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
  // FileHandle is closed automatically when the stream finishes.
}

// Locked FileHandle rejects on second use.
{
  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      // Drain the incoming data so the stream can close cleanly.
      await bytes(stream);
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const fh = await open(testFile, 'r');

  // First use locks the FileHandle.
  const stream1 = await clientSession.createBidirectionalStream({
    body: fh,
  });

  // Second use should reject because it's locked.
  await assert.rejects(
    clientSession.createBidirectionalStream({ body: fh }),
    { code: 'ERR_INVALID_STATE' },
  );

  await Promise.all([stream1.closed, clientSession.closed]);
  await serverEndpoint.close();
  // FileHandle is closed automatically when the stream finishes.
}
