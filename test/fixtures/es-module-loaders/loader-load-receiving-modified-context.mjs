export async function load(url, context, next) {
  console.log(context.foo); // This log is deliberate
  return next(url, context);
}
