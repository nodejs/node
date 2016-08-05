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
    assert.throws(common.mustCall(() => {
      socket.setMulticastInterface('::');
    }));

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {

    // Try to set with an invalid interfaceAddress (wrong Type)
    assert.throws(common.mustCall(() => {
      socket.setMulticastInterface(1);
    }),
    /TypeError/);

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp4');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {

    // Try to set with an invalid interfaceAddress (non-unicast)
    assert.throws(common.mustCall(() => {
      socket.setMulticastInterface('224.0.0.2');
    }));

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp6');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {

    // Try to set with an invalid interfaceAddress ('undefined')
    assert.throws(common.mustCall(() => {
      socket.setMulticastInterface(String(undefined));
    }),
    /EINVAL/);

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp6');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {

    // Try to set with an invalid interfaceAddress ('')
    assert.throws(common.mustCall(() => {
      socket.setMulticastInterface('');
    }),
    /EINVAL/);

    socket.close();
  }));
}

{
  const socket = dgram.createSocket('udp6');

  socket.bind(0);
  socket.on('listening', common.mustCall(() => {

    // An invalid Scope gets turned into #0 (default selection)
    socket.setMulticastInterface('::%%');

    socket.close();
  }));
}

