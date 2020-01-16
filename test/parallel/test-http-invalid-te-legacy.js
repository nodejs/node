// Flags: --http-parser=legacy

'use strict';

// Test https://hackerone.com/reports/735748 is fixed.
// Test should pass with legacy parser, not just the default.

require('../common');

require('./test-http-invalid-te-legacy.js');
