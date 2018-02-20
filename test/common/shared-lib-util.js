/* eslint-disable node-core/required-modules */
'use strict';
const path = require('path');

// If node executable is linked to shared lib, need to take care about the
// shared lib path.
exports.addLibraryPath = function(env) {
  if (!process.config.variables.node_shared) {
    return;
  }

  env = env || process.env;

  env.LD_LIBRARY_PATH =
    (env.LD_LIBRARY_PATH ? env.LD_LIBRARY_PATH + path.delimiter : '') +
    path.join(path.dirname(process.execPath), 'lib.target');
  // For AIX.
  env.LIBPATH =
    (env.LIBPATH ? env.LIBPATH + path.delimiter : '') +
    path.join(path.dirname(process.execPath), 'lib.target');
  // For Mac OSX.
  env.DYLD_LIBRARY_PATH =
    (env.DYLD_LIBRARY_PATH ? env.DYLD_LIBRARY_PATH + path.delimiter : '') +
    path.dirname(process.execPath);
  // For Windows.
  env.PATH =
    (env.PATH ? env.PATH + path.delimiter : '') +
    path.dirname(process.execPath);
};
