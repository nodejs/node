export function resolve(specifier, context, next) {
  return next(specifier, []);
}
