export async function resolve(specifier, context, next) {
  return next(specifier, []);
}
