// Flags: --experimental-quic --no-warnings

// CONNECTION_CLOSE reason strings are truncated to 256 UTF-8 bytes.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const kMaxReasonLength = 256;

// Client close with an ASCII reason longer than the wire limit.
{
  const reason = 'a'.repeat(kMaxReasonLength + 1);
  const expectedReason = reason.slice(0, kMaxReasonLength);
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onerror = mustCall((error) => {
      assert.strictEqual(error.reason, expectedReason);
    });
    await assert.rejects(serverSession.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
      reason: expectedReason,
    });
    serverDone.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await clientSession.opened;
  await setTimeout(100);
  await clientSession.close({
    code: 1n,
    type: 'application',
    reason,
  });

  await serverDone.promise;
  await serverEndpoint.close();
}

// Client close where truncating at the byte limit would split a UTF-8 sequence.
{
  const prefix = 'a'.repeat(kMaxReasonLength - 1);
  const reason = `${prefix}€`;
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    serverSession.onerror = mustCall((error) => {
      assert.strictEqual(error.reason, prefix);
    });
    await assert.rejects(serverSession.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
      reason: prefix,
    });
    serverDone.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
  });
  await clientSession.opened;
  await setTimeout(100);
  await clientSession.close({
    code: 2n,
    type: 'application',
    reason,
  });

  await serverDone.promise;
  await serverEndpoint.close();
}

// Server close exercises the server-specific CONNECTION_CLOSE packet path.
{
  const reason = 's'.repeat(kMaxReasonLength + 1);
  const expectedReason = reason.slice(0, kMaxReasonLength);
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    await setTimeout(100);
    await serverSession.close({
      code: 3n,
      type: 'application',
      reason,
    });
    serverDone.resolve();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    onerror: mustCall((error) => {
      assert.strictEqual(error.reason, expectedReason);
    }),
  });
  await clientSession.opened;
  await assert.rejects(clientSession.closed, {
    code: 'ERR_QUIC_APPLICATION_ERROR',
    reason: expectedReason,
  });

  await serverDone.promise;
  await serverEndpoint.close();
}
