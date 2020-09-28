export async function resolve(specifier, { parentURL }, defaultResolve) {
  if (specifier === 'unknown-builtin-module') {
    return {
      url: 'node:unknown-builtin-module'
    };
  }
  return defaultResolve(specifier, {parentURL}, defaultResolve);
}

export async function getFormat(url, context, defaultGetFormat) {
  if (url === 'node:unknown-builtin-module') {
    return {
      format: 'builtin'
    };
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
