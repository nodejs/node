'use strict';
const common = require('../common');

// Flags: --http-parser=legacy
require('http');

common.expectWarning({
  DeprecationWarning:
    ['The legacy HTTP parser is deprecated.',
     'DEP0131']
});
