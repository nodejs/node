// Flags: --expose-gc

import { platformTimeout } from '../common/index.mjs';
import { gcUntil } from '../common/gc.js';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import { it } from 'node:test';

it('does not retain a timeout that fired before AbortSignal.any()', async () => {
  let timeout = AbortSignal.timeout(1);
  await once(timeout, 'abort');
  const timeoutRef = new WeakRef(timeout);

  const signal = AbortSignal.any([timeout]);
  assert.strictEqual(signal.aborted, true);
  assert.strictEqual(signal.reason, timeout.reason);
  timeout = null;

  await gcUntil('fired timeout is collected', () => timeoutRef.deref() === undefined);
});

it('does not retain a pending timeout when another source has aborted', async () => {
  let timeout = AbortSignal.timeout(platformTimeout(60_000));
  const timeoutRef = new WeakRef(timeout);
  const reason = new Error('aborted');

  const signal = AbortSignal.any([timeout, AbortSignal.abort(reason)]);
  assert.strictEqual(signal.aborted, true);
  assert.strictEqual(signal.reason, reason);
  timeout = null;

  await gcUntil('pending timeout is collected', () => timeoutRef.deref() === undefined);
});
