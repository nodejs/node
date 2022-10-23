'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const assert = require('assert');
const fs = require('fs');

const isWindows = process.platform === 'win32';
const isOSX = process.platform === 'darwin';
const isLinux = process.platform === 'linux';

const watcher = fs.watch(__filename, common.mustNotCall());

if (isOSX || isWindows) {
  watcher.unref();

  setTimeout(
    common.mustCall(() => {
      watcher.ref();
      watcher.unref();
    }),
    common.platformTimeout(100)
  );
}

if (isLinux) {
  assert.throws(() => {
    watcher.ref();
  }, {
    code: 'ERR_FEATURE_UNAVAILABLE_ON_PLATFORM',
  });

  assert.throws(() => {
    watcher.unref();
  }, {
    code: 'ERR_FEATURE_UNAVAILABLE_ON_PLATFORM',
  });
}
