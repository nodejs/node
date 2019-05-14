'use strict';

const common = require('../common');
const util = require('util');

[1, true, false, null, {}].forEach((notString) => {
  common.expectsError(() => util.deprecate(() => {}, 'message', notString), {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "code" argument must be of type string. ' +
             `Received type ${typeof notString}`
  });
});
