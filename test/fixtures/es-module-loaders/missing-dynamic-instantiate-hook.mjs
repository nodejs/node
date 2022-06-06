export function resolve(specifier, { parentURL }, nextResolve) {
  if (specifier === 'test') {
    return {
      url: 'file://'
    };
  }
  return nextResolve(specifier, {parentURL});
}

export function getFormat(url, context, defaultGetFormat) {
  if (url === 'file://') {
    return {
      format: 'dynamic'
    }
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
