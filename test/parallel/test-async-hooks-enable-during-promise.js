'use strict';
const common = require('../common');
const async_hooks = require('async_hooks');

Promise.resolve(1).then(common.mustCall(() => {
  async_hooks.createHook({
    init: common.mustCall(),
    before: common.mustCall(),
    after: common.mustCall(2)
  }).enable();

  process.nextTick(common.mustCall());
}));
