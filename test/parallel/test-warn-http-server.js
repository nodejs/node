'use strict';

const common = require('../common');

// _http_server is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_server module is deprecated.', 'DEPXXXX');

require('_http_server');
