'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const spawnSync = require('child_process').spawnSync;

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);

Object.defineProperty(globalThis, 'interrupt', {
  get: () => {
    return null;
  },
  set: () => {
    throw new Error('should not calling into js');
  },
});

if (process.argv[2] === 'child-busyloop') {
  (function childMain() {
    const addon = require(binding);
    addon[process.argv[3]]();
    while (true) {
      /** wait for interrupt */
    }
  })();
  return;
}

if (process.argv[2] === 'child-idle') {
  (function childMain() {
    const addon = require(binding);
    addon[process.argv[3]]();
    // wait for interrupt
    setTimeout(() => {}, 10_000_000);
  })();
  return;
}

for (const type of ['busyloop', 'idle']) {
  {
    const child = spawnSync(process.execPath, [ __filename, `child-${type}`, 'scheduleInterrupt' ]);
    assert.strictEqual(child.status, 0, `${type} should exit with code 0`);
  }

  {
    const child = spawnSync(process.execPath, [ __filename, `child-${type}`, 'ScheduleInterruptWithJS' ]);
    assert(common.nodeProcessAborted(child.status, child.signal));
  }
}
