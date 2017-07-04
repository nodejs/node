'use strict';
require('../common');

const assert = require('assert');
const http = require('http');

// All of these values should cause http.request() to throw synchronously
// when passed as the value of either options.hostname or options.host
const vals = [{}, [], NaN, Infinity, -Infinity, true, false, 1, 0, new Date()];

const errHostname =
  /^TypeError: "options\.hostname" must either be a string, undefined or null$/;
const errHost =
  /^TypeError: "options\.host" must either be a string, undefined or null$/;

vals.forEach((v) => {
  assert.throws(() => http.request({hostname: v}), errHostname);
  assert.throws(() => http.request({host: v}), errHost);
});

// These values are OK and should not throw synchronously.
// Only testing for 'hostname' validation so ignore connection errors.
const dontCare = () => {};
['', undefined, null].forEach((v) => {
  assert.doesNotThrow(() => {
    http.request({hostname: v}).on('error', dontCare).end();
    http.request({host: v}).on('error', dontCare).end();
  });
});
