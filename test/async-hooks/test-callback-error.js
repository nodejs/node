'use strict';

const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const async_hooks = require('async_hooks');
const initHooks = require('./init-hooks');

switch (process.argv[2]) {
  case 'test_init_callback':
    initHooks({
      oninit: common.mustCall(() => { throw new Error('test_init_callback'); })
    }).enable();

    async_hooks.emitInit(async_hooks.currentId(), 'test_init_callback_type',
                         async_hooks.triggerId());
    break;
  case 'test_callback':
    initHooks({
      onbefore: common.mustCall(() => { throw new Error('test_callback'); })
    }).enable();

    async_hooks.emitInit(async_hooks.currentId(), 'test_callback_type',
                         async_hooks.triggerId());
    async_hooks.emitBefore(async_hooks.currentId());
    break;
}

if (process.execArgv.includes('--abort-on-uncaught-exception')) {
  initHooks({
    oninit: common.mustCall(() => { throw new Error('test_callback_abort'); })
  }).enable();

  async_hooks.emitInit(async_hooks.currentId(), 'test_callback_abort',
                       async_hooks.triggerId());
}

const c1 = spawnSync(`${process.execPath}`, [__filename, 'test_init_callback']);
assert.strictEqual(c1.stderr.toString().split('\n')[0],
                   'Error: test_init_callback');
assert.strictEqual(c1.status, 1);

const c2 = spawnSync(`${process.execPath}`, [__filename, 'test_callback']);
assert.strictEqual(c2.stderr.toString().split('\n')[0], 'Error: test_callback');
assert.strictEqual(c2.status, 1);

const c3 = spawnSync(`${process.execPath}`, ['--abort-on-uncaught-exception',
                                             __filename,
                                             'test_callback_abort']);
assert.strictEqual(c3.stdout.toString(), '');

const stderrOutput = c3.stderr.toString()
                       .trim()
                       .split('\n')
                       .map((s) => s.trim());
assert.strictEqual(stderrOutput[0], 'Error: test_callback_abort');
