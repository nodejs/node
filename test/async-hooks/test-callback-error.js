'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync, fork } = require('child_process');
const async_hooks = require('async_hooks');
const initHooks = require('./init-hooks');

const arg = process.argv[2];
switch (arg) {
  case 'test_init_callback':
    initHooks({
      oninit: common.mustCall(() => { throw new Error(arg); })
    }).enable();
    new async_hooks.AsyncResource(`${arg}_type`);
    return;

  case 'test_callback':
    initHooks({
      onbefore: common.mustCall(() => { throw new Error(arg); })
    }).enable();
    const resource = new async_hooks.AsyncResource(`${arg}_type`);
    resource.emitBefore();
    return;

  case 'test_callback_abort':
    initHooks({
      oninit: common.mustCall(() => { throw new Error(arg); })
    }).enable();
    new async_hooks.AsyncResource(`${arg}_type`);
    return;
}

// this part should run only for the master test
assert.ok(!arg);
{
  // console.log should stay until this test's flakiness is solved
  console.log('start case 1');
  console.time('end case 1');
  const child = spawnSync(process.execPath, [__filename, 'test_init_callback']);
  assert.ifError(child.error);
  const test_init_first_line = child.stderr.toString().split(/[\r\n]+/g)[0];
  assert.strictEqual(test_init_first_line, 'Error: test_init_callback');
  assert.strictEqual(child.status, 1);
  console.timeEnd('end case 1');
}

{
  console.log('start case 2');
  console.time('end case 2');
  const child = spawnSync(process.execPath, [__filename, 'test_callback']);
  assert.ifError(child.error);
  const test_callback_first_line = child.stderr.toString().split(/[\r\n]+/g)[0];
  assert.strictEqual(test_callback_first_line, 'Error: test_callback');
  assert.strictEqual(child.status, 1);
  console.timeEnd('end case 2');
}

{
  console.log('start case 3');
  console.time('end case 3');
  // Timeout is set because this case is known to be problematic, so stderr is
  // logged for further analysis.
  // Ref: https://github.com/nodejs/node/issues/13527
  // Ref: https://github.com/nodejs/node/pull/13559
  const opts = {
    execArgv: ['--abort-on-uncaught-exception'],
    silent: true
  };
  const child = fork(__filename, ['test_callback_abort'], opts);

  let stdout = '';
  child.stdout.on('data', (data) => {
    stdout += data;
  });

  let stderr = '';
  child.stderr.on('data', (data) => {
    stderr += data;
  });

  const tO = setTimeout(() => {
    console.log(stderr);
    child.kill('SIGKILL');
    process.exit(1);
  }, 15 * 1000);
  tO.unref();

  child.on('close', (code, signal) => {
    clearTimeout(tO);
    if (common.isWindows) {
      assert.strictEqual(code, 134);
      assert.strictEqual(signal, null);
    } else {
      assert.strictEqual(code, null);
      // most posix systems will show 'SIGABRT', but alpine34 does not
      if (signal !== 'SIGABRT') {
        console.log(`parent received signal ${signal}\nchild's stderr:`);
        console.log(stderr);
        process.exit(1);
      }
      assert.strictEqual(signal, 'SIGABRT');
    }
    assert.strictEqual(stdout, '');
    const firstLineStderr = stderr.split(/[\r\n]+/g)[0].trim();
    assert.strictEqual(firstLineStderr, 'Error: test_callback_abort');
  });
  console.timeEnd('end case 3');
}
