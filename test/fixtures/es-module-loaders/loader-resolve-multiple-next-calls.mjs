export function resolve(specifier, context, next) {
  const { url: first } = next(specifier, context);
  const { url: second } = next(specifier, context);

  return {
    format: 'module',
    url: first,
  };
}
