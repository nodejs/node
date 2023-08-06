'use strict';

const {
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  JSONStringify,
  SafeSet,
} = primordials;

let debug = require('internal/util/debuglog').debuglog('esm', (fn) => {
  debug = fn;
});

function createImport(impt, index) {
  const imptPath = JSONStringify(impt);
  return `import * as $import_${index} from ${imptPath};
import.meta.imports[${imptPath}] = $import_${index};`;
}

function createExport(expt, index) {
  const nameStringLit = JSONStringify(expt);
  return `let $export_${index};
export { $export_${index} as ${nameStringLit} };
import.meta.exports[${nameStringLit}] = {
  get: () => $export_${index},
  set: (v) => $export_${index} = v,
};`;
}

const createDynamicModule = (imports, exports, url = '', evaluate) => {
  debug('creating ESM facade for %s with exports: %j', url, exports);
  const source = `
${ArrayPrototypeJoin(ArrayPrototypeMap(imports, createImport), '\n')}
${ArrayPrototypeJoin(ArrayPrototypeMap(exports, createExport), '\n')}
import.meta.done();
`;
  const { ModuleWrap } = internalBinding('module_wrap');
  const m = new ModuleWrap(`${url}`, undefined, source, 0, 0);

  const readyfns = new SafeSet();
  const reflect = {
    exports: { __proto__: null },
    onReady: (cb) => { readyfns.add(cb); },
  };

  if (imports.length)
    reflect.imports = { __proto__: null };
  const { registerModule } = require('internal/modules/esm/utils');
  registerModule(m, {
    __proto__: null,
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
