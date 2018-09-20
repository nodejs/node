export function resolve(specifier, parentModule, defaultResolver) {
  if (specifier !== 'test') {
    return defaultResolver(specifier, parentModule);
  }
  return { url: 'file://', format: 'dynamic' };
}
