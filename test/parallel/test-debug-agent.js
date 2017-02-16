'use strict';
const common = require('../common');
const assert = require('assert');
const debug = require('_debug_agent');

assert.throws(
  () => { debug.start(); },
  common.expectsError(
    undefined,
    assert.AssertionError,
    'Debugger agent running without bindings!'
  )
);
