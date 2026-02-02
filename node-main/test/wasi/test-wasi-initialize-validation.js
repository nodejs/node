'use strict';

const common = require('../common');
const assert = require('assert');
const vm = require('vm');
const { WASI } = require('wasi');

const fixtures = require('../common/fixtures');
const bufferSource = fixtures.readSync('simple.wasm');

(async () => {
  {
    // Verify that a WebAssembly.Instance is passed in.
    const wasi = new WASI({ version: 'preview1' });

    assert.throws(
      () => { wasi.initialize(); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance" argument must be of type object/,
      },
    );
  }

  {
    // Verify that the passed instance has an exports objects.
    const wasi = new WASI({ version: 'preview1' });
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', { get() { return null; } });
    assert.throws(
      () => { wasi.initialize(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports" property must be of type object/,
      },
    );
  }

  {
    // Verify that a _initialize() export was passed.
    const wasi = new WASI({ version: 'preview1' });
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _initialize: 5,
          memory: new WebAssembly.Memory({ initial: 1 }),
        };
      },
    });
    assert.throws(
      () => { wasi.initialize(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports\._initialize" property must be of type function/,
      },
    );
  }

  {
    // Verify that a _start export was not passed.
    const wasi = new WASI({ version: 'preview1' });
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _start() {},
          _initialize() {},
          memory: new WebAssembly.Memory({ initial: 1 }),
        };
      },
    });
    assert.throws(
      () => { wasi.initialize(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "instance.exports._start" property must be' +
          ' undefined. Received function _start',
      },
    );
  }

  {
    // Verify that a memory export was passed.
    const wasi = new WASI({ version: 'preview1' });
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() { return { _initialize() {} }; },
    });
    assert.throws(
      () => { wasi.initialize(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports\.memory" property must be a WebAssembly\.Memory object/,
      },
    );
  }

  {
    // Verify that a WebAssembly.Instance from another VM context is accepted.
    const wasi = new WASI({ version: 'preview1' });
    const instance = await vm.runInNewContext(`
      (async () => {
        const wasm = await WebAssembly.compile(bufferSource);
        const instance = await WebAssembly.instantiate(wasm);

        Object.defineProperty(instance, 'exports', {
          get() {
            return {
              _initialize() {},
              memory: new WebAssembly.Memory({ initial: 1 })
            };
          }
        });

        return instance;
      })()
    `, { bufferSource });

    wasi.initialize(instance);
  }

  {
    // Verify that initialize() can only be called once.
    const wasi = new WASI({ version: 'preview1' });
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _initialize() {},
          memory: new WebAssembly.Memory({ initial: 1 }),
        };
      },
    });
    wasi.initialize(instance);
    assert.throws(
      () => { wasi.initialize(instance); },
      {
        code: 'ERR_WASI_ALREADY_STARTED',
        message: /^WASI instance has already started$/,
      },
    );
  }
})().then(common.mustCall());
