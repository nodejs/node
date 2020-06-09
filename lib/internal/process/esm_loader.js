'use strict';

const {
  ArrayPrototypePush,
  ReflectApply,
} = primordials;

const {
  ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING,
} = require('internal/errors').codes;
const { Loader } = require('internal/modules/esm/loader');
const {
  getModuleFromWrap,
} = require('internal/vm/module');
const {
  validateString,
  validateFunction,
} = require('internal/validators');

exports.initializeImportMetaObject = function(wrap, meta) {
  const { callbackMap } = internalBinding('module_wrap');
  if (callbackMap.has(wrap)) {
    const { initializeImportMeta } = callbackMap.get(wrap);
    if (initializeImportMeta !== undefined) {
      initializeImportMeta(meta, getModuleFromWrap(wrap) || wrap);
    }
  }
};

exports.importModuleDynamicallyCallback = async function(wrap, specifier) {
  const { callbackMap } = internalBinding('module_wrap');
  if (callbackMap.has(wrap)) {
    const { importModuleDynamically } = callbackMap.get(wrap);
    if (importModuleDynamically !== undefined) {
      return importModuleDynamically(
        specifier, getModuleFromWrap(wrap) || wrap);
    }
  }
  throw new ERR_VM_DYNAMIC_IMPORT_CALLBACK_MISSING();
};

const createLoader = async () => {
  const loader = new Loader();

  const { getOptionValue } = require('internal/options');
  const userLoaders = getOptionValue('--experimental-loader');

  if (userLoaders.length > 0) {
    const { emitExperimentalWarning } = require('internal/util');
    emitExperimentalWarning('--experimental-loader');

    const importedLoaders = [];
    for (let i = 0; i < userLoaders.length; i += 1) {
      const ns = await loader.importLoader(userLoaders[i]);

      if (ns.resolve !== undefined) {
        validateFunction(ns.resolve, 'resolve');
      }
      if (ns.getFormat !== undefined) {
        validateFunction(ns.getFormat, 'getFormat');
      }
      if (ns.getSource !== undefined) {
        validateFunction(ns.getSource, 'getSource');
      }
      if (ns.transformSource !== undefined) {
        validateFunction(ns.transformSource, 'transformSource');
        ArrayPrototypePush(loader.transformSourceHooks, ns.transformSource);
      }

      if (ns.getGlobalPreloadCode !== undefined) {
        validateFunction(ns.getGlobalPreloadCode, 'getGlobalPreloadCode');
        const code = ReflectApply(ns.getGlobalPreloadCode, undefined, []);
        validateString(code, 'getGlobalPreloadCode return value');
        ArrayPrototypePush(loader.globalPreloadCode, code);
      }

      importedLoaders.push(ns);
    }

    let cursor = {
      resolve: loader._resolve,
      getFormat: loader._getFormat,
      getSource: loader._getSource,
    };
    for (let i = userLoaders.length - 1; i >= 0; i -= 1) {
      const {
        resolve,
        getFormat,
        getSource,
      } = importedLoaders[i];

      const nextLoader = cursor;
      cursor = {
        resolve: resolve ? async (...args) => {
          const result = await resolve(...args, nextLoader.resolve);
          if (result === null) {
            return nextLoader.resolve(...args);
          }
          return result;
        } : nextLoader.resolve,
        getFormat: getFormat ? async (...args) => {
          const result = await getFormat(...args, nextLoader.getFormat);
          if (result === null) {
            return nextLoader.getFormat(...args);
          }
          return result;
        } : nextLoader.getFormat,
        getSource: getSource ? async (...args) => {
          const result = await getSource(...args, nextLoader.getSource);
          if (result === null) {
            return nextLoader.getSource(...args);
          }
          return result;
        } : nextLoader.getSource,
      };
    }

    loader.hook(cursor);
    loader.runGlobalPreloadCode();
  }

  return loader;
};

let processLoader;
exports.getLoader = () => {
  if (!processLoader) {
    processLoader = createLoader();
  }
  return processLoader;
};
