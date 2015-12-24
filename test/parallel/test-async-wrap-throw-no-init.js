'use strict';

require('../common');
const assert = require('assert');
const async_wrap = process.binding('async_wrap');


assert.throws(function() {
  async_wrap.setupHooks(null);
}, /init callback must be a function/);

assert.throws(function() {
  async_wrap.enable();
}, /init callback is not assigned to a function/);

// Should not throw
async_wrap.setupHooks(() => {});
async_wrap.enable();

assert.throws(function() {
  async_wrap.setupHooks(() => {});
}, /hooks should not be set while also enabled/);
