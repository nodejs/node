'use strict';

const common = require('../common');

// _stream_writable is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _stream_writable module is deprecated.', 'DEPXXXX');

require('_stream_writable');
