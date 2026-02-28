export function resolve(a, b, next) {
  if (a === 'exit:') process.exit(42);
  return next(a, b);
}
