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
const kSetMemory = Symbol('setMemory');
const kStarted = Symbol('started');

emitExperimentalWarning('WASI');


class WASI {
  constructor(options = {}) {
    if (options === null || typeof options !== 'object')
      throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

    const { env, preopens } = options;
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

    const wrap = new _WASI(args, envPairs, preopenArray);

    for (const prop in wrap) {
      wrap[prop] = FunctionPrototypeBind(wrap[prop], wrap);
    }

    this[kSetMemory] = wrap._setMemory;
    delete wrap._setMemory;
    this.wasiImport = wrap;
    this[kStarted] = false;
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

    if (exports._start)
      exports._start();
    else if (exports.__wasi_unstable_reactor_start)
      exports.__wasi_unstable_reactor_start();
  }
}


module.exports = { WASI };
