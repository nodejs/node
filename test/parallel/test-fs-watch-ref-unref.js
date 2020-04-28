'use strict';

const common = require('../common');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const fs = require('fs');
const assert = require('assert');

let refCount = 0;

const watcher = fs.watch(__filename, common.mustNotCall());

function unref() {
  watcher.unref();
  refCount--;
  assert.strictEqual(refCount, -1);
}

function ref() {
  watcher.ref();
  refCount++;
  assert.strictEqual(refCount, 0);
}

unref();

setTimeout(
  common.mustCall(() => {
    ref();
    unref();
  }),
  common.platformTimeout(100)
);
