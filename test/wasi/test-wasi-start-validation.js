// Flags: --experimental-wasi-unstable-preview1
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
    const wasi = new WASI();

    assert.throws(
      () => { wasi.start(); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance" argument must be of type object/
      }
    );
  }

  {
    // Verify that the passed instance has an exports objects.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', { get() { return null; } });
    assert.throws(
      () => { wasi.start(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports" property must be of type object/
      }
    );
  }

  {
    // Verify that a _start() export was passed.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return { memory: new Uint8Array() };
      },
    });
    assert.throws(
      () => { wasi.start(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports\._start" property must be of type function/
      }
    );
  }

  {
    // Verify that an _initialize export was not passed.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _start() {},
          _initialize() {},
          memory: new Uint8Array(),
        };
      }
    });
    assert.throws(
      () => { wasi.start(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: 'The "instance.exports._initialize" property must be' +
          ' undefined. Received function _initialize',
      }
    );
  }

  {
    // Verify that a memory export was passed.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() { return { _start() {} }; }
    });
    assert.throws(
      () => { wasi.start(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports\.memory" property must be of type object/
      }
    );
  }

  {
    // Verify that a non-ArrayBuffer memory.buffer is rejected.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _start() {},
          memory: {},
        };
      }
    });
    // The error message is a little white lie because any object
    // with a .buffer property of type ArrayBuffer is accepted,
    // but 99% of the time a WebAssembly.Memory object is used.
    assert.throws(
      () => { wasi.start(instance); },
      {
        code: 'ERR_INVALID_ARG_TYPE',
        message: /"instance\.exports\.memory\.buffer" property must be an WebAssembly\.Memory/
      }
    );
  }

  {
    // Verify that an argument that duck-types as a WebAssembly.Instance
    // is accepted.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _start() {},
          memory: { buffer: new ArrayBuffer(0) },
        };
      }
    });
    wasi.start(instance);
  }

  {
    // Verify that a WebAssembly.Instance from another VM context is accepted.
    const wasi = new WASI({});
    const instance = await vm.runInNewContext(`
      (async () => {
        const wasm = await WebAssembly.compile(bufferSource);
        const instance = await WebAssembly.instantiate(wasm);

        Object.defineProperty(instance, 'exports', {
          get() {
            return {
              _start() {},
              memory: new WebAssembly.Memory({ initial: 1 })
            };
          }
        });

        return instance;
      })()
    `, { bufferSource });

    wasi.start(instance);
  }

  {
    // Verify that start() can only be called once.
    const wasi = new WASI({});
    const wasm = await WebAssembly.compile(bufferSource);
    const instance = await WebAssembly.instantiate(wasm);

    Object.defineProperty(instance, 'exports', {
      get() {
        return {
          _start() {},
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
  }
})().then(common.mustCall());
