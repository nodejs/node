'use strict';

const Transform = require('internal/streams/transform');
module.exports = Transform;

process.emitWarning(
  'The _stream_transform module is deprecated, use stream.Transform instead.',
  'DeprecationWarning', 'DEP0149');
