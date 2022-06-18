export function resolve(specifier, { parentURL, importAssertions }, nextResolve) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: specifier,
      importAssertions,
    };
  }
  return nextResolve(specifier, {parentURL, importAssertions});
}
