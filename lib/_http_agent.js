'use strict';
process.emitWarning(
  'The _http_agent module is deprecated. Please use http',
  'DeprecationWarning', 'DEP00XX');

module.exports = require('internal/http/agent');
