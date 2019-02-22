'use strict';

function assert(value, message) {
  if (!value) {
    require('assert')(value, message);
  }
}

function fail(message) {
  require('assert').fail(message);
}

assert.fail = fail;

module.exports = assert;
