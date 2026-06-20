// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { throws } = assert;

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
  onSessionNewToken() {},
  onSessionKeyLog() {},
  onSessionQlog() {},
  onSessionEarlyDataRejected() {},
  onSessionVersionNegotiation() {},
  onStreamCreated() {},
  onStreamBlocked() {},
  onStreamClose() {},
  onStreamDrain() {},
  onStreamReset() {},
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

// The HTTP/3 application-event callbacks are registered separately (by the
// HTTP/3 consumer layer) and validate their own set.
const http3Callbacks = {
  onSessionApplication() {},
  onSessionGoaway() {},
  onSessionOrigin() {},
  onStreamHeaders() {},
  onStreamTrailers() {},
};
// Fail if any callback is missing
for (const fn of Object.keys(http3Callbacks)) {
  // eslint-disable-next-line no-unused-vars
  const { [fn]: _, ...rest } = http3Callbacks;
  throws(() => quic.setHttp3Callbacks(rest), {
    code: 'ERR_MISSING_ARGS',
  });
}
// If all callbacks are present it should work
quic.setHttp3Callbacks(http3Callbacks);

// Multiple calls should just be ignored.
quic.setHttp3Callbacks(http3Callbacks);
