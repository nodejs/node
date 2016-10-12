'use strict';
require('../common');
const assert = require('assert');
const debug = require('_debug_agent');

assert.throws(
  () => { debug.start();},
  function(err) {
    return (err instanceof assert.AssertionError);
  },
  'Unexpected Error'
);
