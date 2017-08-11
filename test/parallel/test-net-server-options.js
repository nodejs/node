'use strict';
const assert = require('assert');
const common = require('../common');
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
