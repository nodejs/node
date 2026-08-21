export function resolve(specifier, context, next) {
  if (specifier === 'test') {
    return {
      url: 'file://'
    };
  }
  return next(specifier);
}

export function getFormat(url, context, defaultGetFormat) {
  if (url === 'file://') {
    return {
      format: 'dynamic'
    }
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
