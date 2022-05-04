export async function resolve(specifier, context, next) {
  console.log(context.foo); // This log is deliberate
  return next(specifier, context);
}
