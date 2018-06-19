'use strict';

const REPL = require('internal/repl/repl');
const Module = require('module');

const builtinLibs = Module.builtinModules.filter((x) => !/^_|\//.test(x));

builtinLibs.forEach((name) => {
  const setReal = (val) => {
    delete global[name];
    global[name] = val;
  };

  Object.defineProperty(global, name, {
    get: () => {
      const lib = require(name);
      delete global[name];
      Object.defineProperty(global, name, {
        get: () => lib,
        set: setReal,
        configurable: true,
        enumerable: false,
      });

      return lib;
    },
    set: setReal,
    configurable: true,
    enumerable: false,
  });
});

const m = new Module(process.cwd());
m._compile('module.exports = require', process.cwd());
global.require = m.exports;

global.REPL = {
  last: undefined,
  lastError: undefined,
};

// TODO(devsnek): scope this
Object.defineProperties(global, {
  _: {
    enumerable: false,
    configurable: true,
    get: () => global.REPL.last,
    set: (v) => {
      delete global._;
      global._ = v;
    },
  },
  _err: {
    enumerable: false,
    configurable: true,
    get: () => global.REPL.lastError,
    set: (v) => {
      delete global._err;
      global._err = v;
    },
  },
});

new REPL(process.stdout, process.stdin);
