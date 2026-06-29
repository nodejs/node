export async function resolve(specifier, { parentURL, importAttributes }, next) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      shortCircuit: true,
      url: 'file:///asdf',
    };
  }
  return next(specifier);
}

export async function load(url, context, next) {
  if (url === 'file:///asdf') {
    return {
      format: 'esm',
      shortCircuit: true,
      source: '',
    }
  }
  return next(url);
}
