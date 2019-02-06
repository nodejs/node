'use strict';

function assert(value, message) {
  if (!value) {
    require('assert')(value, message);
  }
}

module.exports = assert;
