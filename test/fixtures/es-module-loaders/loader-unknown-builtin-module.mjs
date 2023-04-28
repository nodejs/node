export function resolve(specifier, context, next) {
  if (specifier === 'unknown-builtin-module') return {
    url: 'node:unknown-builtin-module'
  };

  return next(specifier);
}
