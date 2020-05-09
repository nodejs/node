'use strict';

const common = require('../common');

// _tls_common is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _tls_common module is deprecated.', 'DEPXXXX');

require('_tls_common');
