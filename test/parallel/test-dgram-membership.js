'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const multicastAddress = '224.0.0.114';

const setup = dgram.createSocket.bind(dgram, {type: 'udp4', reuseAddr: true});

// addMembership() on closed socket should throw
{
  const socket = setup();
  socket.close(common.mustCall(() => {
    assert.throws(() => {
      socket.addMembership(multicastAddress);
    }, common.expectsError({
      code: 'ERR_SOCKET_DGRAM_NOT_RUNNING',
      type: Error,
      message: /^Not running$/
    }));
  }));
}

// dropMembership() on closed socket should throw
{
  const socket = setup();
  socket.close(common.mustCall(() => {
    assert.throws(() => {
      socket.dropMembership(multicastAddress);
    }, common.expectsError({
      code: 'ERR_SOCKET_DGRAM_NOT_RUNNING',
      type: Error,
      message: /^Not running$/
    }));
  }));
}

// addMembership() with no argument should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.addMembership();
  }, common.expectsError({
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: /^The "multicastAddress" argument must be specified$/
  }));
  socket.close();
}

// dropMembership() with no argument should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.dropMembership();
  }, common.expectsError({
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: /^The "multicastAddress" argument must be specified$/
  }));
  socket.close();
}

// addMembership() with invalid multicast address should throw
{
  const socket = setup();
  assert.throws(() => { socket.addMembership('256.256.256.256'); },
                /^Error: addMembership EINVAL$/);
  socket.close();
}

// dropMembership() with invalid multicast address should throw
{
  const socket = setup();
  assert.throws(() => { socket.dropMembership('256.256.256.256'); },
                /^Error: dropMembership EINVAL$/);
  socket.close();
}

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
