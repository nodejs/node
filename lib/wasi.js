'use strict';
const {
  ArrayPrototypeForEach,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototypeBind,
  ObjectEntries,
  String,
  Symbol,
} = primordials;

const {
  ERR_INVALID_ARG_TYPE,
  ERR_WASI_ALREADY_STARTED
} = require('internal/errors').codes;
const { emitExperimentalWarning } = require('internal/util');
const { isArrayBuffer } = require('internal/util/types');
const {
  validateArray,
  validateBoolean,
  validateFunction,
  validateInt32,
  validateObject,
  validateUndefined,
} = require('internal/validators');
const { WASI: _WASI } = internalBinding('wasi');
const kExitCode = Symbol('kExitCode');
const kSetMemory = Symbol('kSetMemory');
const kStarted = Symbol('kStarted');
const kInstance = Symbol('kInstance');

emitExperimentalWarning('WASI');


function setupInstance(self, instance) {
  validateObject(instance, 'instance');
  validateObject(instance.exports, 'instance.exports');

  // WASI::_SetMemory() in src/node_wasi.cc only expects that |memory| is
  // an object. It will try to look up the .buffer property when needed
  // and fail with UVWASI_EINVAL when the property is missing or is not
  // an ArrayBuffer. Long story short, we don't need much validation here
  // but we type-check anyway because it helps catch bugs in the user's
  // code early.
  validateObject(instance.exports.memory, 'instance.exports.memory');
  if (!isArrayBuffer(instance.exports.memory.buffer)) {
    throw new ERR_INVALID_ARG_TYPE(
      'instance.exports.memory.buffer',
      ['WebAssembly.Memory'],
      instance.exports.memory.buffer);
  }

  self[kInstance] = instance;
  self[kSetMemory](instance.exports.memory);
}

class WASI {
  constructor(options = {}) {
    validateObject(options, 'options');

    if (options.args !== undefined)
      validateArray(options.args, 'options.args');
    const args = ArrayPrototypeMap(options.args || [], String);

    const env = [];
    if (options.env !== undefined) {
      validateObject(options.env, 'options.env');
      ArrayPrototypeForEach(
        ObjectEntries(options.env),
        ({ 0: key, 1: value }) => {
          if (value !== undefined)
            ArrayPrototypePush(env, `${key}=${value}`);
        });
    }

    const preopens = [];
    if (options.preopens !== undefined) {
      validateObject(options.preopens, 'options.preopens');
      ArrayPrototypeForEach(
        ObjectEntries(options.preopens),
        ({ 0: key, 1: value }) =>
          ArrayPrototypePush(preopens, String(key), String(value))
      );
    }

    const { stdin = 0, stdout = 1, stderr = 2 } = options;
    validateInt32(stdin, 'options.stdin', 0);
    validateInt32(stdout, 'options.stdout', 0);
    validateInt32(stderr, 'options.stderr', 0);
    const stdio = [stdin, stdout, stderr];

    const wrap = new _WASI(args, env, preopens, stdio);

    for (const prop in wrap) {
      wrap[prop] = FunctionPrototypeBind(wrap[prop], wrap);
    }

    if (options.returnOnExit !== undefined) {
      validateBoolean(options.returnOnExit, 'options.returnOnExit');
      if (options.returnOnExit)
        wrap.proc_exit = FunctionPrototypeBind(wasiReturnOnProcExit, this);
    }

    this[kSetMemory] = wrap._setMemory;
    delete wrap._setMemory;
    this.wasiImport = wrap;
    this[kStarted] = false;
    this[kExitCode] = 0;
    this[kInstance] = undefined;
  }

  // Must not export _initialize, must export _start
  start(instance) {
    if (this[kStarted]) {
      throw new ERR_WASI_ALREADY_STARTED();
    }
    this[kStarted] = true;

    setupInstance(this, instance);

    const { _start, _initialize } = this[kInstance].exports;

    validateFunction(_start, 'instance.exports._start');
    validateUndefined(_initialize, 'instance.exports._initialize');

    try {
      _start();
    } catch (err) {
      if (err !== kExitCode) {
        throw err;
      }
    }

    return this[kExitCode];
  }

  // Must not export _start, may optionally export _initialize
  initialize(instance) {
    if (this[kStarted]) {
      throw new ERR_WASI_ALREADY_STARTED();
    }
    this[kStarted] = true;

    setupInstance(this, instance);

    const { _start, _initialize } = this[kInstance].exports;

    validateUndefined(_start, 'instance.exports._start');
    if (_initialize !== undefined) {
      validateFunction(_initialize, 'instance.exports._initialize');
      _initialize();
    }
  }
}


module.exports = { WASI };


function wasiReturnOnProcExit(rval) {
  // If __wasi_proc_exit() does not terminate the process, an assertion is
  // triggered in the wasm runtime. Node can sidestep the assertion and return
  // an exit code by recording the exit code, and throwing a JavaScript
  // exception that WebAssembly cannot catch.
  this[kExitCode] = rval;
  throw kExitCode;
}
