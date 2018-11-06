/* eslint-disable node-core/crypto-check */

'use strict';

const common = require('../common');

const http = require('http');

const deprecations = [
  ['The provided URL http://[www.nodejs.org] is not a valid URL, and is supported ' +
  'in the http module solely for compatibility.',
   'DEP0109'],
];
common.expectWarning('DeprecationWarning', deprecations);

const doNotCall = common.mustNotCall(
  'http.request should not connect to http://[www.nodejs.org]'
);

// Ensure that only a single warning is emitted
process.on('warning', common.mustCall());

http.request('http://[www.nodejs.org]', doNotCall).abort();

// Call request() a second time to ensure DeprecationWarning not duplicated
http.request('http://[www.nodejs.org]', doNotCall).abort();
