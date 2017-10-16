'use strict';

require('../common');

const http = require('http');
const assert = require('assert');

assert.throws(() => {
  http.request({
    path: '/thisisinvalid\uffe2'
  }).end();
}, /Request path contains unescaped characters/);
