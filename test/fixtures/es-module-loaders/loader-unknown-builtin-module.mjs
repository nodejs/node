export async function resolve({specifier, parentURL}, defaultResolve, loader) {
  if (specifier === 'unknown-builtin-module') {
    return { url: 'unknown-builtin-module', format: 'builtin' };
  }
  return defaultResolve({specifier, parentURL}, defaultResolve, loader);
}
