'use strict';

require('../common');
const assert = require('assert');
const dgram = require('dgram');
const multicastAddress = '224.0.0.114';

const setup = dgram.createSocket.bind(dgram, { type: 'udp4', reuseAddr: true });

// addMembership() with valid socket and multicast address should not throw
{
  const socket = setup();
  assert.doesNotThrow(() => { socket.addMembership(multicastAddress); });
  socket.close();
}

// dropMembership() without previous addMembership should throw
{
  const socket = setup();
  assert.throws(
    () => { socket.dropMembership(multicastAddress); },
    /^Error: dropMembership EADDRNOTAVAIL$/
  );
  socket.close();
}

// dropMembership() after addMembership() should not throw
{
  const socket = setup();
  assert.doesNotThrow(
    () => {
      socket.addMembership(multicastAddress);
      socket.dropMembership(multicastAddress);
    }
  );
  socket.close();
}
