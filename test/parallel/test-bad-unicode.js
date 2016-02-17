'use strict';
require('../common');
const assert = require('assert');
let exception = null;

try {
  eval('"\\uc/ef"');
} catch (e) {
  exception = e;
}

assert(exception instanceof SyntaxError);
