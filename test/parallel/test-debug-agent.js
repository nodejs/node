'use strict';
const common = require('../common');
const assert = require('assert');
const debug = require('_debug_agent');

assert.throws(
  () => { debug.start(); },
  common.expectsError({ type: assert.AssertionError,
                        message: 'Debugger agent running without bindings!' })
);
