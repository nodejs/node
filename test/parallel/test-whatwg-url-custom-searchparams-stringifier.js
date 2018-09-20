'use strict';

// Tests below are not from WPT.

const common = require('../common');
const URLSearchParams = require('url').URLSearchParams;

{
  const params = new URLSearchParams();
  common.expectsError(() => {
    params.toString.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  });
}
