'use strict';

const PassThrough = require('internal/streams/passthrough');
module.exports = PassThrough;

process.emitWarning(
  'The _stream_passthrough module is deprecated, use stream.PassThrough instead.',
  'DeprecationWarning', 'DEP0149');
