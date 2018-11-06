/* eslint-disable node-core/crypto-check */

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');


const deprecations = [
  ['The provided URL https://[www.nodejs.org] is not a valid URL, and is supported ' +
  'in the https module solely for compatibility.',
   'DEP0109'],
];
common.expectWarning('DeprecationWarning', deprecations);

const doNotCall = common.mustNotCall(
  'https.request should not connect to https://[www.nodejs.org]'
);

// Ensure that only a single warning is emitted
process.on('warning', common.mustCall());

https.request('https://[www.nodejs.org]', doNotCall).abort();

// Call request() a second time to ensure DeprecationWarning not duplicated
https.request('https://[www.nodejs.org]', doNotCall).abort();
