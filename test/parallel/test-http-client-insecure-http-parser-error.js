'use strict';

const common = require('../common');
const assert = require('assert');
const ClientRequest = require('http').ClientRequest;

{
  assert.throws(() => {
    new ClientRequest({insecureHTTPParser: 'wrongValue'});
  }, {
    code: /ERR_INVALID_ARG_TYPE/
  }, 'http request should throw when passing not-boolean value as insecureHTTPParser');
}
