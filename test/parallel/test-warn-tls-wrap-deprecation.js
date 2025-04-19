'use strict';

const common = require('../common');

// _tls_wrap is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _tls_wrap module is deprecated.', 'DEP0192');

require('_tls_wrap');
