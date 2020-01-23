'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

assert.throws(() => net.createServer('path'),
              {
                code: 'ERR_INVALID_ARG_TYPE',
                name: 'TypeError'
              });

assert.throws(() => net.createServer(0),
              {
                code: 'ERR_INVALID_ARG_TYPE',
                name: 'TypeError'
              });
