'use strict';

// Shared Symbols for internal readline modules
const {
  Symbol,
} = primordials;
const kSkipHistory = Symbol('_skipHistory');

module.exports = {
  kSkipHistory,
};
