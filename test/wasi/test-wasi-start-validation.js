// Flags: --experimental-wasi-unstable-preview1
'use strict';

const common = require('../common');
const assert = require('assert');
const { WASI } = require('wasi');

const fixtures = require('../common/fixtures');
const bufferSource = fixtures.readSync('simple.wasm');

{
  const wasi = new WASI();
  assert.throws(
    () => {
      wasi.start();
    },
    { code: 'ERR_INVALID_ARG_TYPE', message: /\bWebAssembly\.Instance\b/ }
  );
}

(async () => {
  const wasi = new WASI({});
  const wasm = await WebAssembly.compile(bufferSource);
  const instance = await WebAssembly.instantiate(wasm);

  assert.throws(
    () => { wasi.start(instance); },
    { code: 'ERR_INVALID_ARG_TYPE', message: /\bWebAssembly\.Memory\b/ }
  );
})();

(async () => {
  const wasi = new WASI();
  const wasm = await WebAssembly.compile(bufferSource);
  const instance = await WebAssembly.instantiate(wasm);
  const values = [undefined, null, 'foo', 42, true, false, () => {}];
  let cnt = 0;

  // Mock instance.exports to trigger start() validation.
  Object.defineProperty(instance, 'exports', {
    get() { return values[cnt++]; }
  });

  values.forEach((val) => {
    assert.throws(
      () => { wasi.start(instance); },
      { code: 'ERR_INVALID_ARG_TYPE', message: /\binstance\.exports\b/ }
    );
  });
})();

(async () => {
  const wasi = new WASI();
  const wasm = await WebAssembly.compile(bufferSource);
  const instance = await WebAssembly.instantiate(wasm);

  // Mock instance.exports.memory to bypass start() validation.
  Object.defineProperty(instance, 'exports', {
    get() {
      return {
        memory: new WebAssembly.Memory({ initial: 1 })
      };
    }
  });

  wasi.start(instance);
  assert.throws(
    () => { wasi.start(instance); },
    {
      code: 'ERR_WASI_ALREADY_STARTED',
      message: /^WASI instance has already started$/
    }
  );
})();

(async () => {
  const wasi = new WASI();
  const wasm = await WebAssembly.compile(bufferSource);
  const instance = await WebAssembly.instantiate(wasm);

  // Mock instance.exports to bypass start() validation.
  Object.defineProperty(instance, 'exports', {
    get() {
      return {
        memory: new WebAssembly.Memory({ initial: 1 }),
        __wasi_unstable_reactor_start: common.mustCall()
      };
    }
  });

  wasi.start(instance);
})();
