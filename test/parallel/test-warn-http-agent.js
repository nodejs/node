'use strict';

const common = require('../common');

// _http_agent is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_agent module is deprecated.', 'DEPXXXX');

require('_http_agent');
