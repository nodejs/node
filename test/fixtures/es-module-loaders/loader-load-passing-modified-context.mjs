export async function load(url, context, next) {
  return next(url, {
    ...context,
    foo: 'bar',
  });
}
