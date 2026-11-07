'use strict';

function getValue(condition) {
  if (condition) {
    return 'truthy';
  }
  /* node:coverage ignore next */
  return 'falsy';
}

module.exports = { getValue };
