'use strict';

const { ModuleWrap, callbackMap } = internalBinding('module_wrap');
const debug = require('util').debuglog('esm');
const ArrayJoin = Function.call.bind(Array.prototype.join);
const ArrayMap = Function.call.bind(Array.prototype.map);

const createDynamicModule = (exports, url = '', evaluate) => {
  debug('creating ESM facade for %s with exports: %j', url, exports);
  const names = ArrayMap(exports, (name) => `${name}`);

  const source = `
${ArrayJoin(ArrayMap(names, (name) =>
    `let $${name};
export { $${name} as ${name} };
import.meta.exports.${name} = {
  get: () => $${name},
  set: (v) => $${name} = v,
};`), '\n')
}

import.meta.done();
`;

  const m = new ModuleWrap(source, `${url}`);
  m.link(() => 0);
  m.instantiate();

  const readyfns = new Set();
  const reflect = {
    namespace: m.namespace(),
    exports: {},
    onReady: (cb) => { readyfns.add(cb); },
  };

  callbackMap.set(m, {
    initializeImportMeta: (meta, wrap) => {
      meta.exports = reflect.exports;
      meta.done = () => {
        evaluate(reflect);
        reflect.onReady = (cb) => cb(reflect);
        for (const fn of readyfns) {
          readyfns.delete(fn);
          fn(reflect);
        }
      };
    },
  });

  return {
    module: m,
    reflect,
  };
};

module.exports = createDynamicModule;
