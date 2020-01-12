export function resolve(specifier, { parentURL }, defaultResolve) {
  if (specifier === 'test') {
    return {
      url: 'file://'
    };
  }
  return defaultResolve(specifier, {parentURL}, defaultResolve);
}

export function getFormat(url, context, defaultGetFormat) {
  if (url === 'file://') {
    return {
      format: 'dynamic'
    }
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
