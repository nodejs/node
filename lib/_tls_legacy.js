'use strict';
process.emitWarning(
  'The _tls_legacy module is deprecated. Please use tls',
  'DeprecationWarning', 'DEP00XX');

module.exports = require('internal/tls/legacy');
