'use strict';
/* global WebAssembly */
const {
  ArrayIsArray,
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
const { WASI: _WASI } = internalBinding('wasi');
const kExitCode = Symbol('exitCode');
const kSetMemory = Symbol('setMemory');
const kStarted = Symbol('started');

emitExperimentalWarning('WASI');


class WASI {
  constructor(options = {}) {
    if (options === null || typeof options !== 'object')
      throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

    const { env, preopens, returnOnExit = false } = options;
    let { args = [] } = options;

    if (ArrayIsArray(args))
      args = ArrayPrototypeMap(args, (arg) => { return String(arg); });
    else
      throw new ERR_INVALID_ARG_TYPE('options.args', 'Array', args);

    const envPairs = [];

    if (env !== null && typeof env === 'object') {
      for (const key in env) {
        const value = env[key];
        if (value !== undefined)
          ArrayPrototypePush(envPairs, `${key}=${value}`);
      }
    } else if (env !== undefined) {
      throw new ERR_INVALID_ARG_TYPE('options.env', 'Object', env);
    }

    const preopenArray = [];

    if (typeof preopens === 'object' && preopens !== null) {
      for (const [key, value] of ObjectEntries(preopens)) {
        ArrayPrototypePush(preopenArray, String(key), String(value));
      }
    } else if (preopens !== undefined) {
      throw new ERR_INVALID_ARG_TYPE('options.preopens', 'Object', preopens);
    }

    if (typeof returnOnExit !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE(
        'options.returnOnExit', 'boolean', returnOnExit);
    }

    const wrap = new _WASI(args, envPairs, preopenArray);

    for (const prop in wrap) {
      wrap[prop] = FunctionPrototypeBind(wrap[prop], wrap);
    }

    if (returnOnExit) {
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

    if (exports === null || typeof exports !== 'object')
      throw new ERR_INVALID_ARG_TYPE('instance.exports', 'Object', exports);

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
