'use strict';

require('../common');
const assert = require('assert');
const net = require('net');

assert.throws(() => {
  net.connect({ port: 8080, autoSelectFamily: 'INVALID' });
}, { code: 'ERR_INVALID_ARG_TYPE' });
