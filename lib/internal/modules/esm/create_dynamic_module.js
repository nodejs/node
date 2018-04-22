'use strict';

const { internalBinding } = require('internal/bootstrap/loaders');
const { ModuleWrap } = internalBinding('module_wrap');
const debug = require('util').debuglog('esm');
const ArrayJoin = Function.call.bind(Array.prototype.join);
const ArrayMap = Function.call.bind(Array.prototype.map);

const createDynamicModule = (exports, url = '', evaluate) => {
  debug(
    `creating ESM facade for ${url} with exports: ${ArrayJoin(exports, ', ')}`
  );
  const names = ArrayMap(exports, (name) => `${name}`);
  // Create two modules: One whose exports are get- and set-able ('reflective'),
  // and one which re-exports all of these but additionally may
  // run an executor function once everything is set up.
  const src = `
  export let executor;
  ${ArrayJoin(ArrayMap(names, (name) => `export let $${name};`), '\n')}
  /* This function is implicitly returned as the module's completion value */
  (() => ({
    setExecutor: fn => executor = fn,
    reflect: {
      exports: { ${
  ArrayJoin(ArrayMap(names, (name) => `
        ${name}: {
          get: () => $${name},
          set: v => $${name} = v
        }`), ', \n')}
      }
    }
  }));`;
  const reflectiveModule = new ModuleWrap(src, `cjs-facade:${url}`);
  reflectiveModule.instantiate();
  const { setExecutor, reflect } = reflectiveModule.evaluate(-1, false)();
  // public exposed ESM
  const reexports = `
  import {
    executor,
    ${ArrayMap(names, (name) => `$${name}`)}
  } from "";
  export {
    ${ArrayJoin(ArrayMap(names, (name) => `$${name} as ${name}`), ', ')}
  }
  if (typeof executor === "function") {
    // add await to this later if top level await comes along
    executor()
  }`;
  if (typeof evaluate === 'function') {
    setExecutor(() => evaluate(reflect));
  }
  const module = new ModuleWrap(reexports, `${url}`);
  module.link(async () => reflectiveModule);
  module.instantiate();
  reflect.namespace = module.namespace();
  return {
    module,
    reflect,
  };
};

module.exports = createDynamicModule;
