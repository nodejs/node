// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(() => true));
const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();

const invalidTypes = ['1', true, null];
const invalidCodes = [
  -1,
  1.5,
  NaN,
  Infinity,
  2 ** 62,
  -1n,
  2n ** 62n,
];

for (const method of ['stopSending', 'resetStream']) {
  for (const code of invalidTypes) {
    assert.throws(() => stream[method](code), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }

  for (const code of invalidCodes) {
    assert.throws(() => stream[method](code), {
      code: 'ERR_OUT_OF_RANGE',
    });
  }
}

for (const code of invalidCodes) {
  assert.throws(() => stream.destroy(new Error('test'), { code }), {
    code: 'ERR_OUT_OF_RANGE',
  });
}
assert.strictEqual(stream.destroyed, false);

stream.destroy();
await stream.closed;
await clientSession.close();
await serverEndpoint.close();
