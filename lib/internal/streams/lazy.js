'use strict';

// These modules export a single function rather than named properties.
// Expose them by name for defineLazyProperties() without loading them together.
module.exports = {
  __proto__: null,
  get Duplex() {
    return require('internal/streams/duplex');
  },
  get Transform() {
    return require('internal/streams/transform');
  },
  get PassThrough() {
    return require('internal/streams/passthrough');
  },
  get duplexPair() {
    return require('internal/streams/duplexpair');
  },
  get compose() {
    return require('internal/streams/compose');
  },
};
