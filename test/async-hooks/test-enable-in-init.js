'use strict';

const common = require('../common');
const async_hooks = require('async_hooks');
const fs = require('fs');

const nestedHook = async_hooks.createHook({
  init: common.mustNotCall()
});
let nestedCall = false;

async_hooks.createHook({
  init: common.mustCall(function(id, type) {
    nestedHook.enable();
    if (!nestedCall) {
      nestedCall = true;
      fs.access(__filename, common.mustCall());
    }
  }, 2)
}).enable();

fs.access(__filename, common.mustCall());
