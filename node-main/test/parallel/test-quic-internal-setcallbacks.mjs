// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import { throws } from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

import { default as binding } from 'internal/test/binding';
const quic = binding.internalBinding('quic');

const callbacks = {
  onEndpointClose() {},
  onSessionNew() {},
  onSessionClose() {},
  onSessionDatagram() {},
  onSessionDatagramStatus() {},
  onSessionHandshake() {},
  onSessionPathValidation() {},
  onSessionTicket() {},
  onSessionVersionNegotiation() {},
  onStreamCreated() {},
  onStreamBlocked() {},
  onStreamClose() {},
  onStreamReset() {},
  onStreamHeaders() {},
  onStreamTrailers() {},
};
// Fail if any callback is missing
for (const fn of Object.keys(callbacks)) {
  // eslint-disable-next-line no-unused-vars
  const { [fn]: _, ...rest } = callbacks;
  throws(() => quic.setCallbacks(rest), {
    code: 'ERR_MISSING_ARGS',
  });
}
// If all callbacks are present it should work
quic.setCallbacks(callbacks);

// Multiple calls should just be ignored.
quic.setCallbacks(callbacks);
