'use strict';

const common = require('../common');

// _http_incoming is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_incoming module is deprecated.', 'DEPXXXX');

require('_http_incoming');
