'use strict';

const util = require('internal/util');

// the sys module was renamed to 'util'.
// this shim remains to keep old programs working.
// sys is deprecated and shouldn't be used

module.exports = require('util');
util.printDeprecationMessage('sys is deprecated. Use util instead.');
