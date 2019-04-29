'use strict';

const { ArrayPrototype, JSON, Object } = primordials;

const debug = require('internal/util/debuglog').debuglog('esm');

const createDynamicModule = (imports, exports, url = '', evaluate) => {
  debug('creating ESM facade for %s with exports: %j', url, exports);
  const names = ArrayPrototype.map(exports, (name) => `${name}`);

  const source = `
${ArrayPrototype.join(ArrayPrototype.map(imports, (impt, index) =>
    `import * as $import_${index} from ${JSON.stringify(impt)};
import.meta.imports[${JSON.stringify(impt)}] = $import_${index};`), '\n')
}
${ArrayPrototype.join(ArrayPrototype.map(names, (name) =>
    `let $${name};
export { $${name} as ${name} };
import.meta.exports.${name} = {
  get: () => $${name},
  set: (v) => $${name} = v,
};`), '\n')
}

import.meta.done();
`;
  const { ModuleWrap, callbackMap } = internalBinding('module_wrap');
  const m = new ModuleWrap(source, `${url}`);

  const readyfns = new Set();
  const reflect = {
    exports: Object.create(null),
    onReady: (cb) => { readyfns.add(cb); },
  };

  if (imports.length)
    reflect.imports = Object.create(null);

  callbackMap.set(m, {
    initializeImportMeta: (meta, wrap) => {
      meta.exports = reflect.exports;
      if (reflect.imports)
        meta.imports = reflect.imports;
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
