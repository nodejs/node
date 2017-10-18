'use strict';

const common = require('../common');
const util = require('util');

[1, true, false, null, {}].forEach((notString) => {
  common.expectsError(() => util.deprecate(() => {}, 'message', notString), {
    type: TypeError,
    message: '`code` argument must be a string'
  });
});
