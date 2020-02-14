'use strict';
/* global WebAssembly */
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
const {
  validateArray,
  validateBoolean,
  validateObject,
} = require('internal/validators');
const { WASI: _WASI } = internalBinding('wasi');
const kExitCode = Symbol('exitCode');
const kSetMemory = Symbol('setMemory');
const kStarted = Symbol('started');

emitExperimentalWarning('WASI');


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

    const wrap = new _WASI(args, env, preopens);

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
  }

  start(instance) {
    if (!(instance instanceof WebAssembly.Instance)) {
      throw new ERR_INVALID_ARG_TYPE(
        'instance', 'WebAssembly.Instance', instance);
    }

    const exports = instance.exports;

    validateObject(exports, 'instance.exports');

    const { memory } = exports;

    if (!(memory instanceof WebAssembly.Memory)) {
      throw new ERR_INVALID_ARG_TYPE(
        'instance.exports.memory', 'WebAssembly.Memory', memory);
    }

    if (this[kStarted]) {
      throw new ERR_WASI_ALREADY_STARTED();
    }

    this[kStarted] = true;
    this[kSetMemory](memory);

    try {
      if (exports._start)
        exports._start();
      else if (exports.__wasi_unstable_reactor_start)
        exports.__wasi_unstable_reactor_start();
    } catch (err) {
      if (err !== kExitCode) {
        throw err;
      }
    }

    return this[kExitCode];
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
