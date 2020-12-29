'use strict';

const common = require('../common');

// _stream_passthrough is deprecated.

common.expectWarning(
  'DeprecationWarning',
  'The _stream_passthrough module is deprecated, use stream.PassThrough instead.',
  'DEP0149');

require('_stream_passthrough');
