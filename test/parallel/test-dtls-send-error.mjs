// Flags: --experimental-dtls --no-warnings

// Test: a record that cannot be put on the wire is reported as such, from the
// flight that could not be sent.
//
// EncOut() discarded SendTo()'s return value, so a datagram the kernel
// refused was simply never sent. Nothing retransmits a record into existence
// when the reason it failed is EMSGSIZE or ENETUNREACH, so the handshake went
// quiet and the only report was a timeout, well after the fact and naming the
// wrong cause.
//
// Reporting it needs somewhere to report it to. The client's first flight ran
// inside the binding's connect(), before the JavaScript session existed, and
// the callback dispatch reaches that session through the handle -- so the
// first flight's error was emitted against a wrapper that was not there yet
// and dropped. What surfaced was the next attempt's, a retransmission
// timeout later. The handshake starts once the wrapper is built, so the
// error now comes from the flight that failed.

import { hasCrypto, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect } = await import('node:dtls');
const dgram = await import('node:dgram');

// Whether this kernel refuses a broadcast send from an unconfigured socket is
// not something the test controls, so establish it with a plain UDP socket
// first. If it does refuse, DTLS has to report that -- deciding from the DTLS
// result alone would let "the error was ignored" look like "the platform
// allowed it", which is exactly the bug.
const refusesBroadcast = await new Promise((resolve) => {
  const probe = dgram.createSocket('udp4');
  probe.send(Buffer.from('x'), 4433, '255.255.255.255', (error) => {
    probe.close();
    resolve(error?.code === 'EACCES');
  });
});

if (!refusesBroadcast) {
  skip('this platform permits sending to the broadcast address');
}

// Sending to the broadcast address without SO_BROADCAST is refused by the
// kernel with EACCES, on the first flight, synchronously.
{
  const timeout = 5000;
  const started = Date.now();

  const session = connect('255.255.255.255', 4433, {
    rejectUnauthorized: false,
    handshakeTimeout: timeout,
  });

  // Reported from the first flight, which connect() runs before it returns:
  // the error path tears the session down, so it is already destroyed here.
  // Dropping that error left the session live until a retransmission
  // rediscovered the same failure.
  assert.strictEqual(session.destroyed, true);

  const error = await session.opened.then(() => null, (e) => e);
  assert.ok(error, 'the handshake should not have completed');

  // The kernel's reason, not "DTLS handshake timeout".
  assert.match(error.message, /permission denied/);

  // Nothing was retransmitted. A retransmission is how this used to be
  // noticed, so needing one means the first flight's error went nowhere.
  assert.strictEqual(session.stats.retransmitCount, 0n);

  // And promptly: noticing at the deadline is what this replaced.
  assert.ok(Date.now() - started < timeout,
            `took ${Date.now() - started}ms of a ${timeout}ms deadline`);

  // A client session owns the endpoint connect() made for it, and the error
  // path closes it. That ownership has to be settled before the handshake
  // starts, now that the handshake can fail before connect() returns; set
  // afterwards, this never settles and the event loop has nothing to drain.
  await session.endpoint.closed;
}
