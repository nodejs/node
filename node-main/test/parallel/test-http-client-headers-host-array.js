'use strict';

require('../common');

const assert = require('assert');
const http = require('http');

{

  const options = {
    port: '80',
    path: '/',
    headers: {
      host: []
    }
  };

  assert.throws(() => {
    http.request(options);
  }, {
    code: /ERR_INVALID_ARG_TYPE/
  }, 'http request should throw when passing array as header host');
}
