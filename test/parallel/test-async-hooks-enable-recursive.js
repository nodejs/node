'use strict';

const common = require('../common');
const async_hooks = require('async_hooks');
const crypto = require('crypto');

const nestedHook = async_hooks.createHook({
  init: common.mustCall()
});

async_hooks.createHook({
  init: common.mustCall((id, type) => {
    nestedHook.enable();
  }, 2)
}).enable();

crypto.randomBytes(1, common.mustCall(() => {
  crypto.randomBytes(1, common.mustCall());
}));
