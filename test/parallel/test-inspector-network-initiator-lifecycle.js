// Flags: --inspect=0 --experimental-network-inspection --expose-internals --expose-gc
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('node:assert');
const { once } = require('node:events');
const { Network, Session } = require('node:inspector/promises');
const { internalBinding } = require('internal/test/binding');
const { gcUntil } = require('../common/gc');
const { NetworkInitiator } = internalBinding('inspector');

async function main() {
  const session = new Session();
  session.connect();
  await session.post('Network.enable');

  function captureRequest() {
    return new NetworkInitiator();
  }

  let initiator = captureRequest();
  const reference = new WeakRef(initiator);
  global.gc();
  // A GC between capture and emission must not invalidate the native snapshot.
  for (let i = 0; i < 2; i++) {
    const event = once(session, 'Network.requestWillBeSent');
    Network.requestWillBeSent({
      requestId: `native-initiator-${i}`,
      timestamp: 1,
      wallTime: 1,
      initiator,
      request: { url: 'http://example.org/', method: 'GET', headers: {} },
    });
    const [{ params }] = await event;
    assert.ok(params.initiator.stack.callFrames.some((frame) =>
      frame.functionName === 'captureRequest' && frame.url === __filename));
  }
  initiator = undefined;
  await gcUntil('emitted initiator is collected', () => reference.deref() === undefined);

  // A snapshot that is never emitted must also be collectable.
  const abandoned = new WeakRef(captureRequest());
  await session.post('Network.disable');
  session.disconnect();
  await gcUntil('abandoned initiator is collected', () => abandoned.deref() === undefined);
}
main().then(common.mustCall());
