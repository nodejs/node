'use strict';
const common = require('../common');
const assert = require('assert');
const url = require('url');

const throwsObjsAndReportTypes = new Map([
  [undefined, 'undefined'],
  [null, 'object'],
  [true, 'boolean'],
  [false, 'boolean'],
  [0, 'number'],
  [function() {}, 'function'],
  [Symbol('foo'), 'symbol']
]);

for (const [urlObject, type] of throwsObjsAndReportTypes) {
  common.expectsError(function() {
    url.format(urlObject);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "urlObject" argument must be one of type Object or string. ' +
             `Received type ${type}`
  });
}

const throwObjsAndReportError = new Map([
  [{
    'host': 'localhost',
    'socketPath': '/mysocket'
  }, {
    code: 'ERR_INVALID_URL_OBJECT_TO_FORMAT_BOTH_HOST_SOCKET',
    type: TypeError,
    message: 'URL scheme not support host and socketPath simultaneous.'
  }],
  [{
    'hostname': 'localhost',
    'port': '23456782'
  }, {
    code: 'ERR_SOCKET_BAD_PORT',
    type: RangeError,
    message: 'Port should be >= 0 and < 65536. Received 23456782.'
  }],
  [{
    'protocol': 'http:',
    'socketPath': '/mysocket'
  }, {
    code: 'ERR_INVALID_PROTOCOL_WITH_SOCKET_PATH',
    type: TypeError,
    message: 'Protocol "http:" not supported with socketPath. ' +
             'Expected one of protocol http+unix or http+unix:'
  }]
]);

for (const [urlObject, expectedError] of throwObjsAndReportError) {
  common.expectsError(function() {
    url.format(urlObject);
  }, expectedError);
}
assert.strictEqual(url.format(''), '');
assert.strictEqual(url.format({}), '');
