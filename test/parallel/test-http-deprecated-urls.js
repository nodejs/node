/* eslint-disable node-core/crypto-check */

'use strict';

const common = require('../common');

const http = require('http');
const modules = { http };

const deprecations = [
  ['The provided URL http://[www.nodejs.org] is not a valid URL, and is supported ' +
  'in the http module solely for compatibility.',
   'DEP0109'],
];

if (common.hasCrypto) {
  const https = require('https');
  modules.https = https;
  deprecations.push(
    ['The provided URL https://[www.nodejs.org] is not a valid URL, and is supported ' +
    'in the https module solely for compatibility.',
     'DEP0109'],
  );
}

common.expectWarning('DeprecationWarning', deprecations);

Object.keys(modules).forEach((module) => {
  const doNotCall = common.mustNotCall(
    `${module}.request should not connect to ${module}://[www.nodejs.org]`
  );
  modules[module].request(`${module}://[www.nodejs.org]`, doNotCall).abort();
});
