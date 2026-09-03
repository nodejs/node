// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel path validation event.
// quic.session.path.validation fires when path validation completes
// during preferred address migration.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const clientChannelFired = Promise.withResolvers();

// Subscribe to the path validation diagnostics channel.
// Verify the client-side event fires with the correct properties.
dc.subscribe('quic.session.path.validation', mustCall((msg) => {
  assert.ok(msg.session, 'message should have session');
  assert.ok(msg.result, 'message should have result');
  assert.ok(msg.newLocalAddress, 'message should have newLocalAddress');
  assert.ok(msg.newRemoteAddress, 'message should have newRemoteAddress');
  if (msg.preferredAddress === true) {
    clientChannelFired.resolve();
  }
}));

const preferredEndpoint = await listen(mustNotCall(), {
  onpathvalidation() {},
  onerror() {},
});

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), {
  transportParams: {
    preferredAddressIpv4: preferredEndpoint.address,
  },
  onpathvalidation() {},
  onerror() {},
});

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
  preferredAddressPolicy: 'use',
  // The onpathvalidation must be set for the JS handler to fire,
  // which in turn publishes to the diagnostics channel.
  onpathvalidation: mustCall(),
});

await Promise.all([clientSession.opened, clientChannelFired.promise]);

await clientSession.close();
await serverEndpoint.close();
await preferredEndpoint.close();
