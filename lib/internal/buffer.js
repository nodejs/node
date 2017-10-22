'use strict';

const binding = process.binding('buffer');
const { setupBufferJS } = binding;

// Remove from the binding so that function is only available as exported here.
// (That is, for internal use only.)
delete binding.setupBufferJS;

// FastBuffer wil be inserted here by lib/buffer.js
module.exports = {
  setupBufferJS
};
