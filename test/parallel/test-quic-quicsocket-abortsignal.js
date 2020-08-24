// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');

const { createQuicSocket } = require('net');

{
  const socket = createQuicSocket();
  const ac = new AbortController();

  // Abort before call.
  ac.abort();

  assert.rejects(socket.connect({ signal: ac.signal }), {
    name: 'AbortError'
  });
  assert.rejects(socket.listen({ signal: ac.signal }), {
    name: 'AbortError'
  });
}

{
  const socket = createQuicSocket();
  const ac = new AbortController();

  assert.rejects(socket.connect({ signal: ac.signal }), {
    name: 'AbortError'
  });
  assert.rejects(socket.listen({ signal: ac.signal }), {
    name: 'AbortError'
  });

  // Abort after call, not awaiting previously created Promises.
  ac.abort();
}

{
  const socket = createQuicSocket();
  const ac = new AbortController();

  async function lookup() {
    ac.abort();
    return { address: '1.1.1.1' };
  }

  assert.rejects(
    socket.connect({ address: 'foo', lookup, signal: ac.signal }), {
      name: 'AbortError'
    });

  assert.rejects(
    socket.listen({
      preferredAddress: { address: 'foo' },
      lookup,
      signal: ac.signal }), {
      name: 'AbortError'
    });
}
