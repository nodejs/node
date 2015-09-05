'use strict';

module.exports = function assert(expression, message) {
  if (!expression)
    throw new Error(`Internal assertion failed: ${message || 'no message'}`);
};
