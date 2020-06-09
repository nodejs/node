export function getFormat(url, context) {
  // Load all .js files as ESM, regardless of package scope
  if (url.endsWith('.js')) {
    return {
      format: 'module'
    }
  }
  return null;
}
