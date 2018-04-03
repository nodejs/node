'use strict';
const common = require('../common');
const net = require('net');

['foobar', 1, {}, []].forEach((input) => connectThrows(input));

// Using port 0 as lookup is emitted before connecting.
function connectThrows(input) {
  const opts = {
    host: 'localhost',
    port: 0,
    lookup: input
  };

  common.expectsError(() => {
    net.connect(opts);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
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
    cb(null, '127.0.0.1', 100);
  });

  s.on('error', common.expectsError({ code: 'ERR_INVALID_ADDRESS_FAMILY' }));
}
