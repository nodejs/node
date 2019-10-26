'use strict';

const common = require('../common');
const async_hooks = require('async_hooks');
const fs = require('fs');

let nestedCall = false;

async_hooks.createHook({
  init: common.mustCall(() => {
    nestedHook.disable();
    if (!nestedCall) {
      nestedCall = true;
      fs.access(__filename, common.mustCall());
    }
  }, 2)
}).enable();

const nestedHook = async_hooks.createHook({
  init: common.mustCall(2)
}).enable();

fs.access(__filename, common.mustCall());
