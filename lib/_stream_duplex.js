'use strict';

const Duplex = require('internal/streams/duplex');
module.exports = Duplex;

process.emitWarning(
  'The _stream_duplex module is deprecated, use stream.Duplex instead.',
  'DeprecationWarning', 'DEP0149');
