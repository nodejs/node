// Flags:  --enable-source-maps

'use strict';
require('../common');
Error.stackTraceLimit = 2;

require('../fixtures/source-map/no-source.js');
