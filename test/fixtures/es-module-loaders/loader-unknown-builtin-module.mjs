export async function resolve(specifier, parent, defaultResolve) {
  if (specifier === 'unknown-builtin-module') {
    return { url: 'unknown-builtin-module', format: 'builtin' };
  }
  return defaultResolve(specifier, parent);
}