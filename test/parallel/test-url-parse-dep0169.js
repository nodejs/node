'use strict';

const common = require('../common');
const url = require('node:url');

// Warning should only happen once per process.
common.expectWarning({
  DeprecationWarning: {
    // eslint-disable-next-line @stylistic/js/max-len
    DEP0169: '`url.parse()` behavior is not standardized and prone to errors that have security implications. Use the WHATWG URL API instead. CVEs are not issued for `url.parse()` vulnerabilities.',
  },
});
url.parse('https://nodejs.org');
