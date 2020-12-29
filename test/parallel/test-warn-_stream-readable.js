'use strict';

const common = require('../common');

// _stream_readable is deprecated.

common.expectWarning(
  'DeprecationWarning',
  'The _stream_readable module is deprecated, use stream.Readable instead.',
  'DEP0149');

require('_stream_readable');
