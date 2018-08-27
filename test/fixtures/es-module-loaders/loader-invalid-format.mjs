export async function resolve(specifier, parentModuleURL, defaultResolve) {
  if (parentModuleURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: 'file:///asdf'
    };
  }
  return defaultResolve(specifier, parentModuleURL);
}
