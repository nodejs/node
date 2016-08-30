'use strict';

// This module is deprecated. Users should be directed towards using
// the specific constants exposed by the individual modules on which they
// are most relevant.
const constants = process.binding('constants');
Object.assign(exports,
              constants.os.errors,
              constants.os.signals,
              constants.fs,
              constants.crypto);

process.emitWarning(
  'The \'constants\' module is deprecated and will be removed soon. ' +
  'Module-specific constants can be accessed via the relevant module.',
  'DeprecationWarning');
