export function getFormat(url, context, defaultGetFormat) {
  // Load all .js files as ESM, regardless of package scope
  if (url.endsWith('.js')) {
    return {
      format: 'module'
    }
  }
  return defaultGetFormat(url, context, defaultGetFormat);
}
