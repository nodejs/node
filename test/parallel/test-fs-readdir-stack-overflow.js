'use strict';

const common = require('../common');

const fs = require('fs');

function recurse() {
  fs.readdirSync('.');
  recurse();
}

common.expectsError(
  () => recurse(),
  {
    type: RangeError,
    message: 'Maximum call stack size exceeded'
  }
);
