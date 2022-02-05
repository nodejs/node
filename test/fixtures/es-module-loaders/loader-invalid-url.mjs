export async function resolve(specifier, { parentURL, importAssertions }, defaultResolve) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: specifier,
      importAssertions,
    };
  }
  return defaultResolve(specifier, {parentURL, importAssertions}, defaultResolve);
}
