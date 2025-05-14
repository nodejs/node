'use strict';

const common = require('../common');

// _stream_transform is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _stream_transform module is deprecated.', 'DEP0193');

require('_stream_transform');
