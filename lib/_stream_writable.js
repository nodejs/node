'use strict';

const Writable = require('internal/streams/writable');
module.exports = Writable;

process.emitWarning(
  'The _stream_writable module is deprecated, use stream.Writable instead.',
  'DeprecationWarning', 'DEP0149');
