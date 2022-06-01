export function resolve(specifier, { parentUrl }, defaultResolve) {
  if (specifier === 'test') {
    return {
      url: 'file://'
    };
  }
  return defaultResolve(specifier, {parentUrl}, defaultResolve);
}

export function getFormat(url, context, defaultGetFormat) {
  if (url === 'file://') {
    return {
      format: 'dynamic'
    }
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
