'use strict';

const I18N = require('internal/messages');
const util = require('internal/util');

// the sys module was renamed to 'util'.
// this shim remains to keep old programs working.
// sys is deprecated and shouldn't be used

module.exports = require('util');
util.printDeprecationMessage(I18N(I18N.SYS_DEPRECATED));
