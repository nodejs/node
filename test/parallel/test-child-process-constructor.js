'use strict';

const common = require('../common');
const assert = require('assert');
const { ChildProcess } = require('child_process');
assert.strictEqual(typeof ChildProcess, 'function');

{
  // Verify that invalid options to spawn() throw.
  const child = new ChildProcess();
  const re = /^TypeError: "options" must be an object$/;

  [undefined, null, 'foo', 0, 1, NaN, true, false].forEach((options) => {
    assert.throws(() => {
      child.spawn(options);
    }, re);
  });
}

{
  // Verify that spawn throws if file is not a string.
  const child = new ChildProcess();
  const re = /^TypeError: "file" must be a string$/;

  [undefined, null, 0, 1, NaN, true, false, {}].forEach((file) => {
    assert.throws(() => {
      child.spawn({ file });
    }, re);
  });
}

{
  // Verify that spawn throws if envPairs is not an array or undefined.
  const child = new ChildProcess();
  const re = /^TypeError: "envPairs" must be an array$/;

  [null, 0, 1, NaN, true, false, {}, 'foo'].forEach((envPairs) => {
    assert.throws(() => {
      child.spawn({ envPairs, stdio: ['ignore', 'ignore', 'ignore', 'ipc'] });
    }, re);
  });
}

{
  // Verify that spawn throws if args is not an array or undefined.
  const child = new ChildProcess();
  const re = /^TypeError: "args" must be an array$/;

  [null, 0, 1, NaN, true, false, {}, 'foo'].forEach((args) => {
    assert.throws(() => {
      child.spawn({ file: 'foo', args });
    }, re);
  });
}

// test that we can call spawn
const child = new ChildProcess();
child.spawn({
  file: process.execPath,
  args: ['--interactive'],
  cwd: process.cwd(),
  stdio: 'pipe'
});

assert.strictEqual(child.hasOwnProperty('pid'), true);
assert(Number.isInteger(child.pid));

// try killing with invalid signal
assert.throws(() => {
  child.kill('foo');
}, common.expectsError({ code: 'ERR_UNKNOWN_SIGNAL' }));

assert.strictEqual(child.kill(), true);
