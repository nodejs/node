'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;
const { spawn } = require('child_process');

const initHooks = require('./init-hooks');

if (process.argv[2] === 'child') {
  initHooks().enable();

  class Foo extends AsyncResource {
    constructor(type) {
      super(type, async_hooks.executionAsyncId());
    }
  }

  [null, undefined, 1, Date, {}, []].forEach((i) => {
    common.expectsError(() => new Foo(i), {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    });
  });

} else {
  const args = process.argv.slice(1).concat('child');
  spawn(process.execPath, args)
    .on('close', common.mustCall((code) => {
      // No error because the type was defaulted
      assert.strictEqual(code, 0);
    }));
}
