'use strict';

const common = require('../common');

// _stream_readable is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _stream_readable module is deprecated. Use `node:stream` instead.', 'DEP0193');

require('_stream_readable');
