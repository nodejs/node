import module from 'module';

const builtins = new Set(
  Object.keys(process.binding('natives')).filter(str =>
    /^(?!(?:internal|node|v8)\/)/.test(str))
);

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

export function resolve(specifier, base, defaultResolver) {
  if (builtins.has(specifier)) {
    return {
      url: `node:${specifier}`,
      format: 'dynamic'
    };
  }
  return defaultResolver(specifier, base);
}
