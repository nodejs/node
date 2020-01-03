import module from 'module';

export function getFormat({ url, defaultGetFormat }) {
  if (module.builtinModules.includes(url)) {
    return {
      format: 'dynamic'
    };
  }
  return defaultGetFormat({url, defaultGetFormat});
}

export function dynamicInstantiate({ url }) {
  const builtinInstance = module._load(url);
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
