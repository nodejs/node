import module from 'module';

export function dynamicInstantiate(url) {
  const builtinInstance = module._load(url.substr(5));
  const builtinExports = ['default', ...Object.keys(builtinInstance)];
  return {
    exports: builtinExports,
    execute: exports => {
      for (let name of builtinExports)
        exports[name].set(builtinInstance[name]);
      exports.default.set(builtinInstance);
    }
  };
}

export function resolve({ specifier, parentURL }, defaultResolve, loader) {
  if (module.builtinModules.includes(specifier)) {
    return {
      url: `node:${specifier}`,
      format: 'dynamic'
    };
  }
  return defaultResolve({specifier, parentURL}, defaultResolve, loader);
}
