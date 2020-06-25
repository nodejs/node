'use strict';
const {
  ArrayPrototypeMap,
  ArrayPrototypePush,
  FunctionPrototypeBind,
  ObjectEntries,
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
  validateInt32,
  validateObject,
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
      for (const [key, value] of ObjectEntries(options.env)) {
        if (value !== undefined)
          ArrayPrototypePush(env, `${key}=${value}`);
      }
    }

    const preopens = [];
    if (options.preopens !== undefined) {
      validateObject(options.preopens, 'options.preopens');
      for (const [key, value] of ObjectEntries(options.preopens)) {
        ArrayPrototypePush(preopens, String(key), String(value));
      }
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

    if (typeof _start !== 'function') {
      throw new ERR_INVALID_ARG_TYPE(
        'instance.exports._start', 'function', _start);
    }
    if (_initialize !== undefined) {
      throw new ERR_INVALID_ARG_TYPE(
        'instance.exports._initialize', 'undefined', _initialize);
    }

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

    if (typeof _initialize !== 'function' && _initialize !== undefined) {
      throw new ERR_INVALID_ARG_TYPE(
        'instance.exports._initialize', 'function', _initialize);
    }
    if (_start !== undefined) {
      throw new ERR_INVALID_ARG_TYPE(
        'instance.exports._start', 'undefined', _initialize);
    }

    if (_initialize !== undefined) {
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
