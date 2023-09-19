'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const multicastAddress = '224.0.0.114';

const setup = dgram.createSocket.bind(dgram, { type: 'udp4', reuseAddr: true });

// addMembership() on closed socket should throw
{
  const socket = setup();
  socket.close(common.mustCall(() => {
    assert.throws(() => {
      socket.addMembership(multicastAddress);
    }, {
      code: 'ERR_SOCKET_DGRAM_NOT_RUNNING',
      name: 'Error',
      message: /^Not running$/
    });
  }));
}

// dropMembership() on closed socket should throw
{
  const socket = setup();
  socket.close(common.mustCall(() => {
    assert.throws(() => {
      socket.dropMembership(multicastAddress);
    }, {
      code: 'ERR_SOCKET_DGRAM_NOT_RUNNING',
      name: 'Error',
      message: /^Not running$/
    });
  }));
}

// addMembership() with no argument should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.addMembership();
  }, {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: /^The "multicastAddress" argument must be specified$/
  });
  socket.close();
}

// dropMembership() with no argument should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.dropMembership();
  }, {
    code: 'ERR_MISSING_ARGS',
    name: 'TypeError',
    message: /^The "multicastAddress" argument must be specified$/
  });
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

// addSourceSpecificMembership with invalid sourceAddress should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.addSourceSpecificMembership(0, multicastAddress);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "sourceAddress" argument must be of type string. ' +
    'Received type number (0)'
  });
  socket.close();
}

// addSourceSpecificMembership with invalid sourceAddress should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.addSourceSpecificMembership(multicastAddress, 0);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "groupAddress" argument must be of type string. ' +
    'Received type number (0)'
  });
  socket.close();
}

// addSourceSpecificMembership with invalid groupAddress should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.addSourceSpecificMembership(multicastAddress, '0');
  }, {
    code: 'EINVAL',
    message: 'addSourceSpecificMembership EINVAL'
  });
  socket.close();
}

// dropSourceSpecificMembership with invalid sourceAddress should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.dropSourceSpecificMembership(0, multicastAddress);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "sourceAddress" argument must be of type string. ' +
    'Received type number (0)'
  });
  socket.close();
}

// dropSourceSpecificMembership with invalid groupAddress should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.dropSourceSpecificMembership(multicastAddress, 0);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "groupAddress" argument must be of type string. ' +
    'Received type number (0)'
  });
  socket.close();
}

// dropSourceSpecificMembership with invalid UDP should throw
{
  const socket = setup();
  assert.throws(() => {
    socket.dropSourceSpecificMembership(multicastAddress, '0');
  }, {
    code: 'EINVAL',
    message: 'dropSourceSpecificMembership EINVAL'
  });
  socket.close();
}
