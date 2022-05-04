export async function load(url, context, next) {
  console.log('load passthru'); // This log is deliberate
  return next(url, context);
}
