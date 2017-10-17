'use strict';

const { ModuleWrap } =
    require('internal/process').internalBinding('module_wrap');
const debug = require('util').debuglog('esm');
const ArrayJoin = Function.call.bind(Array.prototype.join);
const ArrayMap = Function.call.bind(Array.prototype.map);

const createDynamicModule = (exports, url = '', evaluate) => {
  debug(
    `creating ESM facade for ${url} with exports: ${ArrayJoin(exports, ', ')}`
  );
  const names = ArrayMap(exports, (name) => `${name}`);
  // sanitized ESM for reflection purposes
  const src = `export let executor;
  ${ArrayJoin(ArrayMap(names, (name) => `export let $${name}`), ';\n')}
  ;(() => [
    fn => executor = fn,
    { exports: { ${
  ArrayJoin(ArrayMap(names, (name) => `${name}: {
        get: () => $${name},
        set: v => $${name} = v
      }`), ',\n')
} } }
  ]);
  `;
  const reflectiveModule = new ModuleWrap(src, `cjs-facade:${url}`);
  reflectiveModule.instantiate();
  const [setExecutor, reflect] = reflectiveModule.evaluate()();
  // public exposed ESM
  const reexports = `import { executor,
    ${ArrayMap(names, (name) => `$${name}`)}
  } from "";
  export {
    ${ArrayJoin(ArrayMap(names, (name) => `$${name} as ${name}`), ', ')}
  }
  // add await to this later if top level await comes along
  typeof executor === "function" ? executor() : void 0;`;
  if (typeof evaluate === 'function') {
    setExecutor(() => evaluate(reflect));
  }
  const runner = new ModuleWrap(reexports, `${url}`);
  runner.link(async () => reflectiveModule);
  runner.instantiate();
  return {
    module: runner,
    reflect
  };
};

module.exports = {
  createDynamicModule,
  ModuleWrap
};
