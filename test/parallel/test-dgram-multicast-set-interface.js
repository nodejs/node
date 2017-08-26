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
    try {
      socket.setMulticastInterface('::');
      throw new Error('Not detected.');
    } catch (e) {
      console.error(`setMulticastInterface: wrong family error is: ${e}`);
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

if (!common.hasIPv6) {
  common.skip('Skipping udp6 tests, no IPv6 support.');
  return;
}

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
