export async function resolve(specifier, { parentURL, importAssertions }, next) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: specifier,
      importAssertions,
    };
  }
  return next(specifier);
}
