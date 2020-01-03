export function resolve({ specifier, parentURL, defaultResolve }) {
  if (specifier !== 'test') {
    return defaultResolve({specifier, parentURL, defaultResolve});
  }
  return { url: 'file://', format: 'dynamic' };
}
