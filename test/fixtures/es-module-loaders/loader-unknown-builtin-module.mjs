export async function resolve(specifier) {
  if (specifier === 'unknown-builtin-module') {
    return {
      url: 'nodejs:unknown-builtin-module'
    };
  }
  return null;
}

export async function getFormat(url) {
  if (url === 'nodejs:unknown-builtin-module') {
    return {
      format: 'builtin'
    };
  }
  return null;
}
