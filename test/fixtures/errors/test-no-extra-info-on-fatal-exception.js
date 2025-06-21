// Flags: --no-extra-info-on-fatal-exception

'use strict';
require('../../common');
Error.stackTraceLimit = 1;

throw new Error('foo');
