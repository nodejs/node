// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: string body shorthand for stream creation and setBody.
// Strings are automatically encoded as UTF-8.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const decoder = new TextDecoder();

// String body in createBidirectionalStream options.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), 'hello from string body');
      stream.writer.writeSync(new TextEncoder().encode('ok'));
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Body provided as a string — should be UTF-8 encoded automatically.
  const stream = await clientSession.createBidirectionalStream({
    body: 'hello from string body',
  });

  const response = await bytes(stream);
  strictEqual(decoder.decode(response), 'ok');
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// String body with setBody.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), 'setBody string');
      stream.writer.writeSync(new TextEncoder().encode('ok'));
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();
  stream.setBody('setBody string');

  const response = await bytes(stream);
  strictEqual(decoder.decode(response), 'ok');
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// UTF-8 multi-byte characters preserved correctly.
{
  const serverDone = Promise.withResolvers();
  const testString = 'Hello \u{1F600} world \u00E9\u00FC\u00F1';

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const body = await bytes(stream);
      strictEqual(decoder.decode(body), testString);
      stream.writer.writeSync(new TextEncoder().encode('ok'));
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    body: testString,
  });

  const response = await bytes(stream);
  strictEqual(decoder.decode(response), 'ok');
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}
