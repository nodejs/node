export async function resolve(specifier, { parentURL, importAttributes }, next) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: specifier,
      importAttributes,
    };
  }
  return next(specifier);
}
