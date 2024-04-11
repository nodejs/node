'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');

tmpdir.refresh();

fs.mkdirSync(tmpdir.resolve('./parent/child'), { recursive: true });

fs.writeFileSync(tmpdir.resolve('./parent/child/test.tmp'), 'test');

const watcher = fs.watch(tmpdir.resolve('./parent'), { recursive: true }, common.mustCallAtLeast((eventType, filename) => {
  // We are only checking for the filename to avoid having Windows, Linux and Mac specific assertions
  if (fs.readdirSync(tmpdir.resolve('./parent')).length === 0) {
    watcher.close();
  }
}), 1);

// We must wait a bit `fs.rm()` to let the watcher be set up properly
setTimeout(() => {
  fs.rm(tmpdir.resolve('./parent/child'), { recursive: true }, common.mustCall());
}, common.platformTimeout(500));
