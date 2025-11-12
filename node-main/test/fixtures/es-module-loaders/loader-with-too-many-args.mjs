export async function resolve(specifier, context, next) {
  return next(specifier, context, 'resolve-extra-arg');
}

export async function load(url, context, next) {
  return next(url, context, 'load-extra-arg');
}
