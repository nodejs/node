export function resolve({ specifier, parentURL }, defaultResolve, loader) {
  if (specifier !== 'test') {
    return defaultResolve({specifier, parentURL}, defaultResolve, loader);
  }
  return { url: 'file://', format: 'dynamic' };
}
