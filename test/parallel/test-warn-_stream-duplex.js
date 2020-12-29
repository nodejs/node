'use strict';

const common = require('../common');

// _stream_duplex is deprecated.

common.expectWarning(
  'DeprecationWarning',
  'The _stream_duplex module is deprecated, use stream.Duplex instead.',
  'DEP0149');

require('_stream_duplex');
