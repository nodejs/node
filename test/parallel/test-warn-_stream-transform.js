'use strict';

const common = require('../common');

// _stream_transform is deprecated.

common.expectWarning(
  'DeprecationWarning',
  'The _stream_transform module is deprecated, use stream.Transform instead.',
  'DEP0149');

require('_stream_transform');
