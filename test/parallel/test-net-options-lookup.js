'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const expectedError = /^TypeError: "lookup" option should be a function$/;

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
  }, expectedError);
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
