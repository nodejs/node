'use strict';

// Options of node:green_threads for tests only.

const {
  Symbol,
} = primordials;

module.exports = {
  // The `flags` argument of the binding's GreenThread constructor.
  kTestingFlags: Symbol('kTestingFlags'),
};
