/* eslint-disable node-core/required-modules */
export async function resolve(specifier, { parentURL }) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: specifier
    };
  }
  return null;
}
