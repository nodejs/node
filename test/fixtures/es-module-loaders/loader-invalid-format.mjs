export async function resolve(specifier, { parentUrl, importAssertions }, defaultResolve) {
  if (parentUrl && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: 'file:///asdf',
    };
  }
  return defaultResolve(specifier, {parentUrl, importAssertions}, defaultResolve);
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
