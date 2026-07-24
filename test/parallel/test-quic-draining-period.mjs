// Flags: --experimental-quic --no-warnings

// Test: drainingPeriodMultiplier option validation and behavior.
// 1. Default value (3) results in prompt session close after peer closes.
// 2. Custom value is accepted and affects draining duration.
// 3. Values below 3 are clamped to 3.
// 4. Invalid values are rejected.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// Test 1: Default drainingPeriodMultiplier (3) — session closes promptly
// after server closes, not after the full idle timeout.
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    await serverSession.close();
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Measure how long clientSession.closed takes to resolve.
  const start = Date.now();
  await clientSession.closed;
  const elapsed = Date.now() - start;

  // With 3x PTO on localhost (~1-4ms RTT), the draining period should
  // be well under 1 second. The idle timeout is 10 seconds. If the
  // draining period fix is working, elapsed should be much less than 10s.
  ok(elapsed < 2000, `Expected draining to complete in < 2s, took ${elapsed}ms`);

  await serverEndpoint.close();
}

// Test 2: Custom drainingPeriodMultiplier is accepted.
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.opened;
    await serverSession.close();
  }));

  const clientSession = await connect(serverEndpoint.address, {
    drainingPeriodMultiplier: 10,
  });
  await clientSession.opened;

  const start = Date.now();
  await clientSession.closed;
  const elapsed = Date.now() - start;

  // 10x PTO is still very short on localhost. Should complete promptly.
  ok(elapsed < 2000, `Expected draining to complete in < 2s, took ${elapsed}ms`);

  await serverEndpoint.close();
}

// Test 3: Values below 3 are rejected by JS validation.
{
  const serverEndpoint = await listen(mustNotCall());

  await assert.rejects(
    connect(serverEndpoint.address, {
      drainingPeriodMultiplier: 1,
    }),
    { code: 'ERR_OUT_OF_RANGE' },
  );

  await serverEndpoint.close();
}

// Test 4: Invalid types are rejected.
{
  const serverEndpoint = await listen(mustNotCall(), {
    transportParams: { maxIdleTimeout: 1 },
  });

  await assert.rejects(
    connect(serverEndpoint.address, {
      drainingPeriodMultiplier: 'fast',
      transportParams: { maxIdleTimeout: 1 },
    }),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  await assert.rejects(
    connect(serverEndpoint.address, {
      drainingPeriodMultiplier: 300,
      transportParams: { maxIdleTimeout: 1 },
    }),
    { code: 'ERR_OUT_OF_RANGE' },
  );

  await serverEndpoint.close();
}
