// Flags: --experimental-quic --no-warnings

// Test: listEndpoints returns live endpoints filtered by active state.
// - Returns an empty array when no endpoints exist.
// - Includes all created endpoints.
// - active=true (default) filters out destroyed, closing, and busy endpoints.
// - active=false returns all registered endpoints regardless of state.
// - Validates options argument types.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listEndpoints, QuicEndpoint } = await import('node:quic');

// listEndpoints is exported as a function.
assert.strictEqual(typeof listEndpoints, 'function');

// Empty when no endpoints have been created.
assert.deepStrictEqual(listEndpoints(), []);
assert.deepStrictEqual(listEndpoints({ active: false }), []);

// Created endpoints appear in the list.
{
  const ep1 = new QuicEndpoint();
  const ep2 = new QuicEndpoint();

  const list = listEndpoints();
  assert.strictEqual(list.length, 2);
  assert.ok(list.includes(ep1));
  assert.ok(list.includes(ep2));

  // active=false also returns them.
  const all = listEndpoints({ active: false });
  assert.strictEqual(all.length, 2);
  assert.ok(all.includes(ep1));
  assert.ok(all.includes(ep2));

  // Returns plain QuicEndpoint instances, not wrapped in arrays.
  assert.ok(list[0] instanceof QuicEndpoint);
  assert.ok(list[1] instanceof QuicEndpoint);

  await ep1.close();
  await ep2.close();
}

// Destroyed endpoints are excluded by active filter.
{
  const ep = new QuicEndpoint();
  assert.strictEqual(listEndpoints().length, 1);

  await ep.close();
  await ep.closed;
  assert.strictEqual(ep.destroyed, true);

  // Default (active=true) excludes destroyed endpoint.
  assert.strictEqual(listEndpoints().length, 0);

  // active=false still includes it... but destroyed endpoints are removed
  // from the registry entirely in kFinishClose, so it won't appear at all.
  assert.strictEqual(listEndpoints({ active: false }).length, 0);
}

// Busy endpoints are excluded by the active filter.
{
  const ep = new QuicEndpoint();
  assert.strictEqual(listEndpoints().length, 1);

  ep.busy = true;
  assert.strictEqual(ep.busy, true);

  // active=true excludes busy endpoint.
  assert.strictEqual(listEndpoints().length, 0);

  // active=false includes it.
  const all = listEndpoints({ active: false });
  assert.strictEqual(all.length, 1);
  assert.ok(all.includes(ep));

  ep.busy = false;

  // After un-busying, it reappears in the active list.
  assert.strictEqual(listEndpoints().length, 1);

  await ep.close();
}

// Options validation.
assert.throws(() => listEndpoints('bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => listEndpoints(null), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => listEndpoints({ active: 'yes' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => listEndpoints({ active: 1 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
