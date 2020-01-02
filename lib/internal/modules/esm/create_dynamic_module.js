'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  JSONStringify,
  ObjectCreate,
  Set,
} = primordials;

const debug = require('internal/util/debuglog').debuglog('esm');

function createImport(impt, index) {
  const imptPath = JSONStringify(impt);
  return `import * as $import_${index} from ${imptPath};
import.meta.imports[${imptPath}] = $import_${index};`;
}

function createExport(expt) {
  const name = `${expt}`;
  return `let $${name};
export { $${name} as ${name} };
import.meta.exports.${name} = {
  get: () => $${name},
  set: (v) => $${name} = v,
};`;
}

const createDynamicModule = (imports, exports, url = '', evaluate) => {
  debug('creating ESM facade for %s with exports: %j', url, exports);
  const source = `
${ArrayPrototypeJoin(ArrayPrototypeMap(imports, createImport), '\n')}
${ArrayPrototypeJoin(ArrayPrototypeMap(exports, createExport), '\n')}
import.meta.done();
`;
  const { ModuleWrap, callbackMap } = internalBinding('module_wrap');
  const m = new ModuleWrap(`${url}`, undefined, source, 0, 0);

  const readyfns = new Set();
  const reflect = {
    exports: ObjectCreate(null),
    onReady: (cb) => { readyfns.add(cb); },
  };

  if (imports.length)
    reflect.imports = ObjectCreate(null);

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
