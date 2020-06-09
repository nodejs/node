export async function resolve(specifier, { parentURL }) {
  if (parentURL && specifier === '../fixtures/es-modules/test-esm-ok.mjs') {
    return {
      url: 'file:///asdf'
    };
  }
  return null;
}

export function getFormat(url) {
  if (url === 'file:///asdf') {
    return {
      format: 'esm'
    }
  }
  return null;
}
