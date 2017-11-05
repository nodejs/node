'use strict';
process.emitWarning(
  'The _stream_duplex module is deprecated. Please use stream',
  'DeprecationWarning', 'DEP00XX');

module.exports = require('internal/streams/duplex');
