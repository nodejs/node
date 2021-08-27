export async function resolve(specifier, { parentURL, importAssertions }, defaultResolve) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: 'file:///asdf'
    };
  }
  return defaultResolve(specifier, {parentURL, importAssertions}, defaultResolve);
}

export async function load(url, context, next) {
  if (url === 'file:///asdf') {
    return {
      format: 'esm',
      source: '',
    }
  }
  return next(url, context, next);
}
