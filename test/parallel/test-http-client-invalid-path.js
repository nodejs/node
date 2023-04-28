'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

assert.throws(() => {
  http.request({
    path: '/thisisinvalid\uffe2'
  }).end();
}, {
  code: 'ERR_UNESCAPED_CHARACTERS',
  name: 'TypeError'
});
