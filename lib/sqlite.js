'use strict';
const {
  emitExperimentalWarning,
  SymbolDispose,
} = require('internal/util');
const binding = internalBinding('sqlite');

emitExperimentalWarning('SQLite');

// TODO(cjihrig): Move this to C++ once Symbol.dispose reaches Stage 4.
binding.DatabaseSync.prototype[SymbolDispose] = function() {
  try {
    this.close();
  } catch {
    // Ignore errors.
  }
};

module.exports = binding;
