'use strict';

const common = require('../common');

// _http_client is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_client module is deprecated.', 'DEPXXXX');

require('_http_client');
