'use strict';

// Tests below are not from WPT.

const common = require('../common');
const { URLSearchParams } = require('url');

{
  const params = new URLSearchParams();
  common.expectsError(() => {
    params.forEach.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  });
}
