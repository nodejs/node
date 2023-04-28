// Flags:  --enable-source-maps

'use strict';
require('../common');
Error.stackTraceLimit = 5;

require('../fixtures/source-map/enclosing-call-site-min.js');
