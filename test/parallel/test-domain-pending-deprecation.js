// Flags: --pending-deprecation --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const config = process.binding('config');

assert(config.pendingDeprecation);

const depWarnMessage = 'The "domain" module has been deprecated and should ' +
                       'not be used. A replacement mechanism has not yet ' +
                       'been determined.';

common.expectWarning('DeprecationWarning', depWarnMessage);

// Verify that requiring the domain module emits a deprecation warning
// when the --pending-deprecation flag is set
require('domain');
