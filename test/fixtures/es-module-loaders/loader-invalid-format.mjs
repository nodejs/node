export async function resolve(specifier, { parentURL, importAssertions }, defaultResolve) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: 'file:///asdf',
    };
  }
  return defaultResolve(specifier, {parentURL, importAssertions}, defaultResolve);
}

export async function load(url, context, next) {
  if (url === 'file:///asdf') {
    return {
      format: 'esm',
      shortCircuit: true,
      source: '',
    }
  }
  return next(url, context, next);
}
