// Flags: --expose-internals --no-warnings
'use strict';

// See also test/sequential/test-async-wrap-getasyncid.js

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { TTYWRAP } = internalBinding('async_wrap').Providers;
const tty_wrap = internalBinding('tty_wrap');
const providers = { TTYWRAP };

// Make sure that the TTYWRAP Provider is tested.
{
  const hooks = require('async_hooks').createHook({
    init(id, type) {
      if (type === 'NONE')
        throw new Error('received a provider type of NONE');
      delete providers[type];
    },
  }).enable();
  process.on('beforeExit', common.mustCall(() => {
    process.removeAllListeners('uncaughtException');
    hooks.disable();

    const objKeys = Object.keys(providers);
    if (objKeys.length > 0)
      process._rawDebug(objKeys);
    assert.strictEqual(objKeys.length, 0);
  }));
}

function testInitialized(req, ctor_name) {
  assert.strictEqual(typeof req.getAsyncId, 'function');
  assert(Number.isSafeInteger(req.getAsyncId()));
  assert(req.getAsyncId() > 0);
  assert.strictEqual(req.constructor.name, ctor_name);
}

{
  const ttyFd = common.getTTYfd();

  const handle = new tty_wrap.TTY(ttyFd, false);
  testInitialized(handle, 'TTY');
}
