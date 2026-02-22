'use strict';
// Source file for testing that branch coverage respects ignore comments

function getValue(condition) {
  if (condition) {
    return 'truthy';
  }
  /* node:coverage ignore next */
  return 'falsy';
}

module.exports = { getValue };
