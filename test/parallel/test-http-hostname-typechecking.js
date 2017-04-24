'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// All of these values should cause http.request() to throw synchronously
// when passed as the value of either options.hostname or options.host
const vals = [{}, [], NaN, Infinity, -Infinity, true, false, 1, 0, new Date()];

function errCheck(name) {
  return new RegExp(`^TypeError: "options\\.${name}" must either be a ` +
                    'string, undefined or null$');
}

vals.forEach((v) => {
  assert.throws(() => http.request({hostname: v}), errCheck('hostname'));
  assert.throws(() => http.request({host: v}), errCheck('host'));
});

// These values are OK and should not throw synchronously
['', undefined, null].forEach((v) => {
  assert.doesNotThrow(() => {
    http.request({hostname: v}).on('error', common.noop).end();
    http.request({host: v}).on('error', common.noop).end();
  });
});
