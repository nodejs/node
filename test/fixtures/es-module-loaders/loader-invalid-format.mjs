export async function resolve({ specifier, parentURL, defaultResolve }) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: 'file:///asdf'
    };
  }
  return defaultResolve({specifier, parentURL, defaultResolve});
}
