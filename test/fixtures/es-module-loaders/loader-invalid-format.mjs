export async function resolve(specifier, { parentURL }, defaultResolve) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: 'file:///asdf'
    };
  }
  return defaultResolve(specifier, {parentURL}, defaultResolve);
}

export function getFormat(url, context, defaultGetFormat) {
  if (url === 'file:///asdf') {
    return {
      format: 'esm'
    }
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
