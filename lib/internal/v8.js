'use strict';

// This file provides access to some of V8's native runtime functions. See
// https://github.com/v8/v8/wiki/Built-in-functions for further information
// about their implementation.
// They have to be loaded before anything else to make sure we deactivate them
// before executing any other code. Gaining access is achieved by using a
// specific flag that is used internally in the startup phase.

// Clone the provided Map Iterator.
function previewMapIterator(it) {
  return %MapIteratorClone(it);
}

// Clone the provided Set Iterator.
function previewSetIterator(it) {
  return %SetIteratorClone(it);
}

module.exports = {
  previewMapIterator,
  previewSetIterator
};
