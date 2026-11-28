'use strict';
const common = require('../common');

// A directory below a recursive fs.watch() root that is removed and created
// again before its events are handled must still be watched afterwards.
// Refs: https://github.com/nodejs/node/issues/67592

if (!common.isLinux)
  common.skip('the recursive watcher is native on this platform');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const root = tmpdir.resolve('root');
const sub = path.join(root, 'sub');
fs.mkdirSync(sub, { recursive: true });

const watcher = fs.watch(root, { recursive: true });
let step = 0;

function recreate(name) {
  fs.rmSync(sub, { recursive: true, force: true });
  fs.mkdirSync(sub);
  fs.writeFileSync(path.join(sub, name), name);
}

watcher.on('change', common.mustCallAtLeast((eventType, filename) => {
  if (step === 0 && filename === path.join('sub', 'a.txt')) {
    step = 1;
    setTimeout(() => recreate('b.txt'), common.platformTimeout(100));
  } else if (step === 1 && filename === path.join('sub', 'b.txt')) {
    step = 2;
    setTimeout(() => {
      fs.writeFileSync(path.join(sub, 'c.txt'), 'c');
    }, common.platformTimeout(100));
  } else if (step === 2 && filename === path.join('sub', 'c.txt')) {
    assert.strictEqual(eventType, 'rename');
    step = 3;
    watcher.close();
  }
}));

setTimeout(() => recreate('a.txt'), common.platformTimeout(100));

process.on('exit', () => {
  assert.strictEqual(step, 3);
});
