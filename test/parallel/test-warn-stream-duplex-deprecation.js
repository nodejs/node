'use strict';

const common = require('../common');

// _stream_duplex is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _stream_duplex module is deprecated.', 'DEP0193');

require('_stream_duplex');
