export function getFormat({ url, defaultGetFormat }) {
  // Load all .js files as ESM, regardless of package scope
  if (url.endsWith('.js')) {
    return {
      format: 'module'
    }
  }
  return defaultGetFormat({url, defaultGetFormat});
}
