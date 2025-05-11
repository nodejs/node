'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

// _tls_common is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _tls_common module is deprecated.', 'DEP0192');

require('_tls_common');
