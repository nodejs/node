'use strict';
require('../common');
const assert = require('assert');
const debug = require('_debug_agent');

assert.throws(
  () => { debug.start();},
   function(err) {
     if ((err instanceof assert.AssertionError) && /value/.test(err)) {
       return true;
     }
   }
);
