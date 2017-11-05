'use strict';
process.emitWarning(
  'The _stream_transform module is deprecated. Please use stream',
  'DeprecationWarning', 'DEP00XX');

module.exports = require('internal/streams/transform');
