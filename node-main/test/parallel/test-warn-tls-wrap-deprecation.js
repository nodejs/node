'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

// _tls_wrap is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _tls_wrap module is deprecated. Use `node:tls` instead.', 'DEP0192');

require('_tls_wrap');
