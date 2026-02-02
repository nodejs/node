'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Explicitly request default system selection
    socket.setMulticastInterface('0.0.0.0');

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    socket.close(common.mustCall(() => {
      assert.throws(() => { socket.setMulticastInterface('0.0.0.0'); },
                    /Not running/);
    }));
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Try to set with an invalid interfaceAddress (wrong address class)
    //
    // This operation succeeds on some platforms, throws `EINVAL` on some
    // platforms, and throws `ENOPROTOOPT` on others. This is unpleasant, but
    // we should at least test for it.
    try {
      socket.setMulticastInterface('::');
    } catch (e) {
      assert(e.code === 'EINVAL' || e.code === 'ENOPROTOOPT');
    }

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Try to set with an invalid interfaceAddress (wrong Type)
    assert.throws(() => {
      socket.setMulticastInterface(1);
    }, /TypeError/);

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Try to set with an invalid interfaceAddress (non-unicast)
    assert.throws(() => {
      socket.setMulticastInterface('224.0.0.2');
    }, /Error/);

    socket.close();
  }));
}

// If IPv6 is not supported, skip the rest of the test. However, don't call
// common.skip(), which calls process.exit() while there is outstanding
// common.mustCall() activity.
if (!common.hasIPv6)
  return;

{
  const socket = dgram.createSocket('udp6');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Try to set with an invalid interfaceAddress ('undefined')
    assert.throws(() => {
      socket.setMulticastInterface(String(undefined));
    }, /EINVAL/);

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp6');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Try to set with an invalid interfaceAddress ('')
    assert.throws(() => {
      socket.setMulticastInterface('');
    }, /EINVAL/);

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp6');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {
    // Using lo0 for OsX, on all other OSes, an invalid Scope gets
    // turned into #0 (default selection) which is also acceptable.
    socket.setMulticastInterface('::%lo0');

    socket.close();
  }));
}
