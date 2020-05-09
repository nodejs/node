'use strict';

const common = require('../common');

// _http_outgoing is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_outgoing module is deprecated.', 'DEPXXXX');

require('_http_outgoing');
