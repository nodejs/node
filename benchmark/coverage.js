'use strict';

const fs = require('node:fs');
const path = require('node:path');
const Mod = require('node:module');

const benchmarkFolder = __dirname;
const dir = fs.readdirSync(path.join(__dirname, '../lib'));

const allModuleExports = {};

function getCallSite() {
  const originalStackFormatter = Error.prepareStackTrace;
  Error.prepareStackTrace = (err, stack) => {
    // Some benchmarks change the stackTraceLimit if so, get the last line
    // TODO: check if it matches the benchmark folder
    if (stack.length >= 2) {
      return `${stack[2].getFileName()}:${stack[2].getLineNumber()}`;
    }
    return stack;
  };

  const err = new Error();
  err.stack; // eslint-disable-line no-unused-expressions
  Error.prepareStackTrace = originalStackFormatter;
  return err.stack;
}

const skippedFunctionClasses = [
  'EventEmitter',
  'Worker',
  'ClientRequest',
  'Readable',
  'StringDecoder',
  'TLSSocket',
  'MessageChannel',
];

const skippedModules = [
  'node:cluster',
  'node:trace_events',
  'node:stream/promises',
];

function fetchModules(allModuleExports) {
  for (const f of dir) {
    if (f.endsWith('.js') && !f.startsWith('_')) {
      const moduleName = `node:${f.slice(0, f.length - 3)}`;
      if (skippedModules.includes(moduleName)) {
        continue;
      }
      const exports = require(moduleName);
      allModuleExports[moduleName] = Object.assign({}, exports);

      for (const fnKey of Object.keys(exports)) {
        if (typeof exports[fnKey] === 'function' && !fnKey.startsWith('_')) {
          if (
            exports[fnKey].toString().match(/^class/) ||
            skippedFunctionClasses.includes(fnKey)
          ) {
            // Skip classes for now
            continue;
          }
          const originalFn = exports[fnKey];
          allModuleExports[moduleName][fnKey] = function() {
            const callerStr = getCallSite();
            if (typeof callerStr === 'string' && callerStr.startsWith(benchmarkFolder) &&
              callerStr.replace(benchmarkFolder, '').match(/^\/.+\/.+/)) {
              if (!allModuleExports[moduleName][fnKey]._called) {
                allModuleExports[moduleName][fnKey]._called = 0;
              }
              allModuleExports[moduleName][fnKey]._called++;


              if (!allModuleExports[moduleName][fnKey]._calls) {
                allModuleExports[moduleName][fnKey]._calls = [];
              }
              allModuleExports[moduleName][fnKey]._calls.push(callerStr);
            }
            return originalFn.apply(exports, arguments);
          };
        }
      }
    }
  }
}

fetchModules(allModuleExports);

const req = Mod.prototype.require;
Mod.prototype.require = function(id) {
  let newId = id;
  if (!id.startsWith('node:')) {
    newId = `node:${id}`;
  }
  const data = allModuleExports[newId];
  if (!data) {
    return req.apply(this, arguments);
  }
  return data;
};

process.on('beforeExit', () => {
  for (const module of Object.keys(allModuleExports)) {
    for (const fn of Object.keys(allModuleExports[module])) {
      if (allModuleExports[module][fn]?._called) {
        const _fn = allModuleExports[module][fn];
        process.send({
          type: 'coverage',
          module,
          fn,
          times: _fn._called,
        });
      }
    }
  }
});
