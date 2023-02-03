export async function resolve(specifier, context, next) {
  // This check is needed to make sure that we don't prevent the
  // resolution from follow-up loaders. It wouldn't be a problem
  // in real life because loaders aren't supposed to break the
  // resolution, but the ones used in our tests do, for convenience.
  if (specifier.includes('loader')) {
    return next(specifier);
  }

  console.log('resolve passthru'); // This log is deliberate
  return next(specifier);
}
