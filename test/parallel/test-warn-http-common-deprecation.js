'use strict';

const common = require('../common');

// _http_common is deprecated.

common.expectWarning('DeprecationWarning',
                     'The _http_common module is deprecated.', 'DEP0195');

require('_http_common');
