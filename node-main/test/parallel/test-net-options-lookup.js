'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

['foobar', 1, {}, []].forEach((input) => connectThrows(input));

// Using port 0 as lookup is emitted before connecting.
function connectThrows(input) {
  const opts = {
    host: 'localhost',
    port: 0,
    lookup: input
  };

  assert.throws(() => {
    net.connect(opts);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });
}

connectDoesNotThrow(() => {});

function connectDoesNotThrow(input) {
  const opts = {
    host: 'localhost',
    port: 0,
    lookup: input
  };

  return net.connect(opts);
}

{
  // Verify that an error is emitted when an invalid address family is returned.
  const s = connectDoesNotThrow((host, options, cb) => {
    if (options.all) {
      cb(null, [{ address: '127.0.0.1', family: 100 }]);
    } else {
      cb(null, '127.0.0.1', 100);
    }
  });

  s.on('error', common.expectsError({
    code: 'ERR_INVALID_ADDRESS_FAMILY',
    host: 'localhost',
    port: 0,
    message: 'Invalid address family: 100 localhost:0'
  }));
}
