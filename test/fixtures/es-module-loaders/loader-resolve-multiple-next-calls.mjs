export async function resolve(specifier, context, next) {
  const { url: first } = await next(specifier, context);
  const { url: second } = await next(specifier, context);

  return {
    format: 'module',
    url: first,
  };
}
