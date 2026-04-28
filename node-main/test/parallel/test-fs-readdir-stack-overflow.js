'use strict';

require('../common');

const assert = require('assert');
const fs = require('fs');

function recurse() {
  fs.readdirSync('.');
  recurse();
}

assert.throws(
  () => recurse(),
  {
    name: 'RangeError',
    message: 'Maximum call stack size exceeded'
  }
);
