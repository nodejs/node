'use strict';

const common = require('../common');
const async_hooks = require('async_hooks');
const fs = require('fs');

const nestedHook = async_hooks.createHook({
  init: common.mustCall()
});

async_hooks.createHook({
  init: common.mustCall((id, type) => {
    nestedHook.enable();
  }, 2)
}).enable();

fs.access(__filename, common.mustCall(() => {
  fs.access(__filename, common.mustCall());
}));
