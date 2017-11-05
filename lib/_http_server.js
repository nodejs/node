'use strict';
process.emitWarning(
  'The _http_server module is deprecated. Please use http',
  'DeprecationWarning', 'DEP00XX');

module.exports = require('internal/http/server');
