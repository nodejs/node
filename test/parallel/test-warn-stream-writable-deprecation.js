'use strict';

const common = require('../common');

// _stream_writable is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _stream_writable module is deprecated.', 'DEP0193');

require('_stream_writable');
