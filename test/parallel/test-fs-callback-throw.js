'use strict';
// Refs: https://github.com/nodejs/node/issues/65667
// Exceptions thrown after an fs operation completes must reach
// 'uncaughtException' instead of being swallowed.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();
const dir = tmpdir.path;
const file = path.join(dir, 'file');
const link = path.join(dir, 'link');
fs.writeFileSync(file, '');

// The callback throws.
const callbackCases = {
  'mkdtemp': (cb) => fs.mkdtemp(path.join(dir, 'x-'), cb),
  'realpath.native': (cb) => fs.realpath.native(dir, cb),
  'mkdir recursive': (cb) => fs.mkdir(path.join(dir, 'a', 'b'), { recursive: true }, cb),
  'readdir recursive': (cb) => fs.readdir(dir, { recursive: true }, cb),
};
if (common.canCreateSymLink()) {
  fs.symlinkSync(file, link);
  callbackCases.readlink = (cb) => fs.readlink(link, cb);
}

// A nextTick callback scheduled after the promise settles throws.
const promiseCases = {
  'promises.mkdtemp': () => fs.promises.mkdtemp(path.join(dir, 'p-')),
  'dir.read': async () => {
    const d = await fs.promises.opendir(dir);
    await d.read();
    process.nextTick(() => d.closeSync());
  },
};

const cases = [
  ...Object.entries(callbackCases).map(([name, run]) => [name, () => {
    run(common.mustSucceed(() => { throw new Error(name); }));
  }]),
  ...Object.entries(promiseCases).map(([name, run]) => [name, async () => {
    await run();
    process.nextTick(() => { throw new Error(name); });
  }]),
];

let current;
process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, current);
  next();
}, cases.length));

function next() {
  const entry = cases.shift();
  if (entry === undefined) return;
  current = entry[0];
  entry[1]();
}

next();
