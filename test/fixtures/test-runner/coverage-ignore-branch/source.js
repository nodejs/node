'use strict';

function getValue(condition) {
  if (condition) {
    return 'truthy';
  }
  /* node:coverage ignore next */
  return 'falsy';
}

// This function has a branch where the ignored line is mixed with
// non-ignored uncovered code, so the branch should still be reported.
function getMixed(condition) {
  if (condition) {
    return 'yes';
  }
  /* node:coverage ignore next */
  const ignored = 'ignored';
  return ignored + ' no';
}

module.exports = { getValue, getMixed };
