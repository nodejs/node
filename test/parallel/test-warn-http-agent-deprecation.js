'use strict';

const common = require('../common');

// _http_agent is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_agent module is deprecated. Use `node:http` instead.', 'DEP0195');

require('_http_agent');
