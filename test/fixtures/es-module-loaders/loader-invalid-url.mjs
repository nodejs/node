/* eslint-disable node-core/required-modules */
export async function resolve(specifier, { parentURL }, defaultResolve) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: specifier
    };
  }
  return defaultResolve(specifier, {parentURL}, defaultResolve);
}
