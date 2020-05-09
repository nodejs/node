'use strict';

const common = require('../common');

// _tls_wrap is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _tls_wrap module is deprecated.', 'DEPXXXX');

require('_tls_wrap');
