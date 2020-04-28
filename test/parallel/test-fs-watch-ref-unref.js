'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const fs = require('fs');

const watcher = fs.watch(__filename, common.mustNotCall());

watcher.unref();

setTimeout(
  common.mustCall(() => {
    watcher.ref();
    watcher.unref();
  }),
  common.platformTimeout(100)
);
