// The --expose-http2 flag is not set
'use strict';

require('../common');
const assert = require('assert');

assert.throws(() => require('http2'), // eslint-disable-line crypto-check
              /^Error: Cannot find module 'http2'$/);
