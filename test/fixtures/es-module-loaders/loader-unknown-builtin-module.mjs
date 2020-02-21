export async function resolve(specifier, { parentURL }, defaultResolve) {
  if (specifier === 'unknown-builtin-module') {
    return {
      url: 'nodejs:unknown-builtin-module'
    };
  }
  return defaultResolve(specifier, {parentURL}, defaultResolve);
}

export async function getFormat(url, context, defaultGetFormat) {
  if (url === 'nodejs:unknown-builtin-module') {
    return {
      format: 'builtin'
    };
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
