export async function resolve(specifier, context, next) {
  console.log('resolve passthru'); // This log is deliberate
  return next(specifier, context);
}
