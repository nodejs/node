export async function resolve(specifier, { parentUrl, importAssertions }, defaultResolve) {
  if (parentUrl && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: specifier,
      importAssertions,
    };
  }
  return defaultResolve(specifier, {parentUrl, importAssertions}, defaultResolve);
}
