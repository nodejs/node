'use strict';

const common = require('../common');

// _http_client is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_client module is deprecated. Use `node:http` instead.', 'DEP0199');

require('_http_client');
