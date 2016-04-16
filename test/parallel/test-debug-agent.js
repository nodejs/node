'use strict';
require('../common');
const assert = require('assert');

assert.throws(() => { require('_debug_agent').start(); },
  assert.AssertionError);
