export async function resolve(specifier, context, next) {
  console.log('resolve foo'); // This log is deliberate
  return next('file:///foo.mjs', context);
}
