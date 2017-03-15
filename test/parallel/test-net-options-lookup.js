'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const expectedError = /^TypeError: "lookup" option should be a function$/;

['foobar', 1, {}, []].forEach((input) => connectThrows(input));

function connectThrows(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  assert.throws(function() {
    net.connect(opts);
  }, expectedError);
}

[() => {}].forEach((input) => connectDoesNotThrow(input));

function connectDoesNotThrow(input) {
  const opts = {
    host: 'localhost',
    port: common.PORT,
    lookup: input
  };

  assert.doesNotThrow(function() {
    net.connect(opts);
  });
}
