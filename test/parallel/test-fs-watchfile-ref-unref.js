'use strict';

const common = require('../common');

const fs = require('fs');
const assert = require('assert');

let refCount = 0;

const watcher = fs.watchFile(__filename, common.mustNotCall());

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
    watcher.unref();
    watcher.ref();
    watcher.ref();
    watcher.unref();
  }),
  common.platformTimeout(100)
);
