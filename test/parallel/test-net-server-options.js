'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

assert.throws(function() { net.createServer('path'); },
              common.expectsError({
                code: 'ERR_INVALID_ARG_TYPE',
                type: TypeError
              }));

assert.throws(function() { net.createServer(0); },
              common.expectsError({
                code: 'ERR_INVALID_ARG_TYPE',
                type: TypeError
              }));
