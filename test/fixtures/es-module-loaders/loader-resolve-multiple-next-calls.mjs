export async function resolve(specifier, context, next) {
  const { url: first } = await next(specifier);
  const { url: second } = await next(specifier);

  return {
    format: 'module',
    url: first,
  };
}
