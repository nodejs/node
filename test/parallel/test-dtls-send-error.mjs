// Flags: --experimental-dtls --no-warnings

// Test: a record that cannot be put on the wire is reported as such.
//
// EncOut() discarded SendTo()'s return value, so a datagram the kernel
// refused was simply never sent. Nothing retransmits a record into existence
// when the reason it failed is EMSGSIZE or ENETUNREACH, so the handshake went
// quiet and the only report was a timeout, well after the fact and naming the
// wrong cause.

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

  const error = await session.opened.then(() => null, (e) => e);
  assert.ok(error, 'the handshake should not have completed');

  // The kernel's reason, not "DTLS handshake timeout".
  assert.match(error.message, /permission denied/);

  // And promptly: noticing at the deadline is what this replaced.
  assert.ok(Date.now() - started < timeout,
            `took ${Date.now() - started}ms of a ${timeout}ms deadline`);
}
