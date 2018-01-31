'use strict';
const common = require('../common');
const http = require('http');

common.expectsError(() => {
  http.request({
    path: '/thisisinvalid\uffe2'
  }).end();
}, {
  code: 'ERR_UNESCAPED_CHARACTERS',
  type: TypeError
});
