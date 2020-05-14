import module from 'module';

export function getFormat(url, context, defaultGetFormat) {
  if (module.builtinModules.includes(url)) {
    return {
      format: 'dynamic'
    };
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}

export function dynamicInstantiate(url) {
  const builtinInstance = module._load(url);
  const builtinExports = Object.keys(builtinInstance);
  return {
    exports: builtinExports.concat('default'),
    execute: exports => {
      for (let name of builtinExports)
        exports[name].set(builtinInstance[name]);
      exports.default.set(builtinInstance);
    }
  };
}
