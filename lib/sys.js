'use strict';

const util = require('util');

// the sys module was renamed to 'util'.
// this shim remains to keep old programs working.
// sys is deprecated and shouldn't be used

const setExports = util.deprecate(function() {
  module.exports = util;
}, 'sys is deprecated. Use util instead.');

setExports();
