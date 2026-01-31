'use strict';

const common = require('../common');

// _http_outgoing is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_outgoing module is deprecated. Use `node:http` instead.', 'DEP0199');

require('_http_outgoing');
