// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel path validation event.
// quic.session.path.validation fires when path validation completes
// during preferred address migration.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const clientChannelFired = Promise.withResolvers();

// Subscribe to the path validation diagnostics channel.
// Verify the client-side event fires with the correct properties.
dc.subscribe('quic.session.path.validation', (msg) => {
  ok(msg.session, 'message should have session');
  ok(msg.result, 'message should have result');
  ok(msg.newLocalAddress, 'message should have newLocalAddress');
  ok(msg.newRemoteAddress, 'message should have newRemoteAddress');
  if (msg.preferredAddress === true) {
    clientChannelFired.resolve();
  }
});

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
  // The onpathvalidation must be set for the JS handler to fire,
  // which in turn publishes to the diagnostics channel.
  onpathvalidation: mustCall(),
});

await Promise.all([clientSession.opened, clientChannelFired.promise]);

await clientSession.close();
await serverEndpoint.close();
await preferredEndpoint.close();
