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

  assert.doesNotThrow(() => {
    net.connect(opts);
  });
}
