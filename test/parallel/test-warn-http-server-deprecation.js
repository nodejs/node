'use strict';

const common = require('../common');

// _http_outgoing is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_server module is deprecated. Use `node:http` instead.', 'DEP0195');

require('_http_server');
