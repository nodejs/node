'use strict';

const common = require('../common');

const fs = require('fs');
const assert = require('assert');

const uncalledListener = common.mustNotCall();
const uncalledListener2 = common.mustNotCall();
const watcher = fs.watchFile(__filename, uncalledListener);

watcher.unref();
watcher.unref();
watcher.ref();
watcher.unref();
watcher.ref();
watcher.ref();
watcher.unref();

fs.unwatchFile(__filename, uncalledListener);

// Watch the file with two different listeners.
fs.watchFile(__filename, uncalledListener);
const watcher2 = fs.watchFile(__filename, uncalledListener2);

setTimeout(
  common.mustCall(() => {
    fs.unwatchFile(__filename, common.mustNotCall());
    assert.strictEqual(watcher2.listenerCount('change'), 2);
    fs.unwatchFile(__filename, uncalledListener);
    assert.strictEqual(watcher2.listenerCount('change'), 1);
    watcher2.unref();
  }),
  common.platformTimeout(100)
);
