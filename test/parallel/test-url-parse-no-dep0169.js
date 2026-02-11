'use strict';

const common = require('../common');
const url = require('node:url');

process.on('warning', common.mustNotCall());

// Both `url.resolve` and `url.format` should not trigger warnings even if
// they internally depend on `url.parse`.
url.resolve('https://nodejs.org', '/dist');
url.format({
  protocol: 'https',
  hostname: 'nodejs.org',
  pathname: '/some/path',
  query: {
    page: 1,
    format: 'json',
  },
});
