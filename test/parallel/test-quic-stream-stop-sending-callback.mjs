// Flags: --experimental-quic --no-warnings

// A STOP_SENDING callback is emitted only on the endpoint that receives
// the frame. It is distinct from the RESET_STREAM callback.

import {
  hasQuic,
  mustCall,
  mustNotCall,
  skip,
} from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const stopCode = 99n;
const callbackReceived = Promise.withResolvers();
const resetReceived = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Sending STOP_SENDING locally must not echo an onstopsending callback
    // back to this stream.
    stream.onstopsending = mustNotCall();
    const closed = assert.rejects(stream.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
    stream.onreset = mustCall((error) => {
      assert.strictEqual(error.errorCode, stopCode);
      resetReceived.resolve();
    });
    stream.stopSending(stopCode);
    await closed;
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
stream.onreset = mustNotCall();
stream.onstopsending = mustCall((error) => {
  assert.strictEqual(error.errorCode, stopCode);
  assert.strictEqual(stream.writer.canWrite, null);
  assert.strictEqual(stream.writer.writeSync(new Uint8Array([2])), false);
  callbackReceived.resolve();
});

stream.writer.writeSync(new Uint8Array([1]));
await Promise.all([callbackReceived.promise, resetReceived.promise]);

clientSession.destroy();
await clientSession.closed;
await serverEndpoint.close();
