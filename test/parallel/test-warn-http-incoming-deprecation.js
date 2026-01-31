'use strict';

const common = require('../common');

// _http_incoming is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_incoming module is deprecated. Use `node:http` instead.', 'DEP0199');

require('_http_incoming');
