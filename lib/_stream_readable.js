'use strict';

const Readable = require('internal/streams/readable');
module.exports = Readable;

process.emitWarning(
  'The _stream_readable module is deprecated, use stream.Readable instead.',
  'DeprecationWarning', 'DEP0149');
